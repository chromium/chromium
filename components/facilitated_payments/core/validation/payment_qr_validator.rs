// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Validates EMV Merchant-Presented QR codes, the format used by payment QR
// codes such as Pix: https://www.emvco.com/emv-technologies/qr-codes/
// - `get_pix_qr_code_type()` validates a Pix code and returns its type.
// - `validate_payment_qr_code()` identifies the payment scheme of a code and
//   validates it, including its CRC.

const PIX_GLOBALLY_UNIQUE_IDENTIFIER: &str = "br.gov.bcb.pix";

const PAYLOAD_FORMAT_INDICATOR_DATA_OBJECT_ID: &str = "00";
const MERCHANT_ACCOUNT_INFORMATION_DATA_OBJECT_ID: &str = "26";
// ID of the Pix key data object nested in the merchant account information data
// object. Mandatory for static QR codes.
const MERCHANT_ACCOUNT_INFORMATION_STATIC_KEY_DATA_OBJECT_ID: &str = "01";
// ID of the 3-URL data object nested in the merchant account information data
// object. Mandatory for dynamic QR codes.
const MERCHANT_ACCOUNT_INFORMATION_DYNAMIC_URL_DATA_OBJECT_ID: &str = "25";
const ADDITIONAL_DATA_FIELD_TEMPLATE_DATA_OBJECT_ID: &str = "62";
const CRC16_DATA_OBJECT_ID: &str = "63";

pub enum PixQrCodeType {
    Dynamic,
    Static,
}

pub enum PixQrCodeError {
    NotPaymentCode,
    MissingPayloadFormatIndicator,
    InvalidMerchantPresentedCode,
    MissingGloballyUniqueIdentifier,
    NonPixMerchantPresentedCode,
    EmptyAdditionalDataFieldTemplate,
    NonFinalCrc,
    UnknownPixCodeType,
}

pub type PixQrCodeResult = Result<PixQrCodeType, PixQrCodeError>;

/// Parses and determines the `PixQrCodeType` that `code` represents, or `None`
/// if `code` is not a valid Pix code.
pub fn get_pix_qr_code_type(code: &[u8]) -> PixQrCodeResult {
    type Error = PixQrCodeError;

    if code.is_empty() {
        return Err(Error::NotPaymentCode);
    }

    // Per the specification, EMV Merchant-Presented QR codes can contain three
    // types data object values:
    // - Numeric: encoded as the ASCII digits from "0" to "9"
    // - Alphanumeric Special: 96 characters, i.e. the "Common Character Set" as
    //   defined in [EMV Book 4]. Interestingly enough, the Merchant-Presented
    //   QR code specification claims 96 characters, while EMV Book 4 only
    //   claims 95 characters. The characters in question are the characters
    //   that are common to all ISO 8859 encodings, with codepoints from 0x20 to
    //   0x7F.
    // - Unicode: encoded as UTF-8
    //
    // So any `code` that is not valid UTF-8 can be ignored.
    let code = match str::from_utf8(code) {
        Ok(code) => code,
        Err(_) => return Err(Error::NotPaymentCode),
    };

    // 4.6.1.1 The Payload Format Indicator (ID "00") shall be the first data
    // object in the QR Code.
    let Some((first_data_object, mut rest)) = parse_next_data_object(code) else {
        return Err(Error::NotPaymentCode);
    };
    if first_data_object.id != PAYLOAD_FORMAT_INDICATOR_DATA_OBJECT_ID {
        return Err(Error::MissingPayloadFormatIndicator);
    }

    let mut detected_qr_code_type = None;

    loop {
        let Some((next_data_object, next_rest)) = parse_next_data_object(rest) else {
            return Err(Error::InvalidMerchantPresentedCode);
        };
        match next_data_object.id {
            MERCHANT_ACCOUNT_INFORMATION_DATA_OBJECT_ID => {
                let qr_code_type =
                    get_type_from_merchant_account_info_data_object(next_data_object)?;
                if detected_qr_code_type.is_none() {
                    detected_qr_code_type = Some(qr_code_type);
                }
            }
            ADDITIONAL_DATA_FIELD_TEMPLATE_DATA_OBJECT_ID
                // 4.8.1.1: If present, the Additional Data Field Template shall contain at
                // least 1 data object.
                if !contains_valid_data_objects(next_data_object.value) => {
                    return Err(Error::EmptyAdditionalDataFieldTemplate);
                }
            CRC16_DATA_OBJECT_ID => {
                // 4.6.1.2 The CRC (ID "63") shall be the last data object in the QR Code
                if !next_rest.is_empty() {
                    return Err(Error::NonFinalCrc);
                }
                // If the `detected_qr_code_type` is still `None`, that means the parser never
                // saw a merchant account info data object. Assume this is a
                // non-Pix merchant-presented code.
                return detected_qr_code_type.ok_or(Error::NonPixMerchantPresentedCode);
            }
            _ => (),
        }
        rest = next_rest;
    }
}

struct Section<'a> {
    id: &'a str,
    value: &'a str,
}

fn parse_next_data_object(input: &str) -> Option<(Section<'_>, &str)> {
    // 3.2: Each data object is made up of three individual fields [...]:
    // - The ID is coded as a two-digit numeric value, with a value ranging from
    //   "00" to "99",
    // - The length is coded as a two-digit numeric value, with a value ranging
    //   from "01" to "99"
    // - The value field has a minimum length of one character and maximum
    //   length of 99 characters.
    let (id, rest) = input.split_at_checked(2)?;
    if id.chars().any(|c| !c.is_ascii_digit()) {
        return None;
    }
    let (length, rest) = rest.split_at_checked(2)?;
    if length.starts_with('+') {
        return None;
    }
    let length: usize = length.parse().ok()?;
    let (value, rest) = rest.split_at_checked(length)?;
    Some((Section { id, value }, rest))
}

fn contains_valid_data_objects(input: &str) -> bool {
    let mut input = input;

    // Subtle: this means that an empty `input` will return false
    while let Some((_data_object, rest)) = parse_next_data_object(input) {
        if rest.is_empty() {
            return true;
        }
        input = rest;
    }
    false
}

/// Given a merchant account information `data_object`, returns the
/// PixQrCodeType it represents or None if the `data_object` is invalid or it
/// does not represent a Pix code.
fn get_type_from_merchant_account_info_data_object(
    data_object: Section,
) -> Result<PixQrCodeType, PixQrCodeError> {
    type Error = PixQrCodeError;

    // 4.7.11.2: If present, a Merchant Account Information template shall
    // contain a primitive Globally Unique Identifier data object with a
    // data object ID "00", as defined in Table 4.2.
    let Some((gui_data_object, rest)) = parse_next_data_object(data_object.value) else {
        return Err(Error::InvalidMerchantPresentedCode);
    };
    if gui_data_object.id != "00" {
        return Err(Error::MissingGloballyUniqueIdentifier);
    }

    if !gui_data_object.value.eq_ignore_ascii_case(PIX_GLOBALLY_UNIQUE_IDENTIFIER) {
        return Err(Error::NonPixMerchantPresentedCode);
    }

    // 4.7.11.2: The value of the Globally Unique Identifier sets the context
    // for the remainder of the template and the meaning of the other data
    // objects in the template are context specific and outside of the scope
    // of EMVCo.
    let Some((next_data_object, rest)) = parse_next_data_object(rest) else {
        return Err(Error::InvalidMerchantPresentedCode);
    };

    // Make sure the remainder consists of valid data_objects if there is still
    // unparsed content.
    if !rest.is_empty() && !contains_valid_data_objects(rest) {
        return Err(Error::InvalidMerchantPresentedCode);
    }

    // According to EMVCo:
    // 3.2: The position of all other data objects under the root or within
    // templates is arbitrary and may appear in any order.
    //
    // While Pix does not appear to mandate a specific ordering for data
    // objects, in practice, the validator expects the first data object
    // after the Globally Unique Identifier to indicate whether the Pix code
    // is static or dynamic. If it does not, treat the Pix code as
    // malformed.
    match next_data_object.id {
        MERCHANT_ACCOUNT_INFORMATION_DYNAMIC_URL_DATA_OBJECT_ID => Ok(PixQrCodeType::Dynamic),
        MERCHANT_ACCOUNT_INFORMATION_STATIC_KEY_DATA_OBJECT_ID => Ok(PixQrCodeType::Static),
        _ => Err(Error::UnknownPixCodeType),
    }
}

// Application identifiers of the supported eWallet payment schemes, compared
// case-insensitively. Pix uses `PIX_GLOBALLY_UNIQUE_IDENTIFIER` instead.
const DUITNOW_APPLICATION_IDENTIFIER: &str = "A0000006150001";
// PromptPay uses one identifier for credit transfers and another for bill
// payments, which merchants use at checkout.
const PROMPTPAY_CREDIT_TRANSFER_APPLICATION_IDENTIFIER: &str = "A000000677010111";
const PROMPTPAY_BILL_PAYMENT_APPLICATION_IDENTIFIER: &str = "A000000677010112";
// QRIS templates use reverse domain names under `.co.id` as their identifiers,
// e.g. "ID.CO.QRIS.WWW" for the national template, or the issuer's domain for
// the issuer templates. Compared case-insensitively.
const QRIS_GLOBALLY_UNIQUE_IDENTIFIER_PREFIX: &str = "ID.CO.";
// eWallet merchant account information templates with their own reverse domain
// name identifiers, compared case-insensitively.
const SHOPEEPAY_GLOBALLY_UNIQUE_IDENTIFIER: &str = "MY.COM.AIRPAY.WWW";
const MOMO_GLOBALLY_UNIQUE_IDENTIFIER: &str = "vn.momo";

// 4.7.11: Merchant account information templates with a Globally Unique
// Identifier use IDs "26" to "51". IDs "02" to "25" are reserved for payment
// networks and carry no identifier.
const FIRST_TEMPLATE_MERCHANT_ACCOUNT_INFORMATION_ID: u8 = 26;
const LAST_TEMPLATE_MERCHANT_ACCOUNT_INFORMATION_ID: u8 = 51;
const GLOBALLY_UNIQUE_IDENTIFIER_DATA_OBJECT_ID: &str = "00";

// 4.7.3: The CRC is CRC-16/CCITT-FALSE (polynomial 0x1021, initial value
// 0xFFFF) over the whole payload up to and including the CRC ID and length,
// encoded as four hexadecimal characters.
const CRC16_POLYNOMIAL: u16 = 0x1021;
const CRC16_INITIAL_VALUE: u16 = 0xFFFF;
const CRC16_HIGH_BIT: u16 = 0x8000;
const CRC16_VALUE_LENGTH: usize = 4;
const BITS_PER_BYTE: usize = 8;

pub enum PaymentQrCodeScheme {
    Pix,
    DuitNow,
    PromptPay,
    Qris,
    ShopeePay,
    MoMo,
}

pub enum PaymentQrCodeValidationResult {
    /// A valid code of a scheme other than Pix.
    Valid,
    ValidPixStatic,
    ValidPixDynamic,
    NotPaymentCode,
    UnsupportedScheme,
    FailedTlvParsing,
    FailedCrcCheck,
    EmptyAdditionalDataFieldTemplate,
}

/// The scheme and the validation result are separate, so that a failure can
/// still be attributed to the scheme of the code that failed.
pub struct PaymentQrCodeValidation {
    /// The scheme of the first merchant account information template with a
    /// supported identifier, found even if the code fails validation, or
    /// `None` if there is no such template.
    pub scheme: Option<PaymentQrCodeScheme>,
    pub result: PaymentQrCodeValidationResult,
}

/// Parses `code` as an EMV Merchant-Presented QR code, identifies its payment
/// scheme, and validates it, including its CRC.
pub fn validate_payment_qr_code(code: &[u8]) -> PaymentQrCodeValidation {
    type ValidationResult = PaymentQrCodeValidationResult;
    const NOT_PAYMENT_CODE: PaymentQrCodeValidation =
        PaymentQrCodeValidation { scheme: None, result: ValidationResult::NotPaymentCode };

    // See `get_pix_qr_code_type()`: any `code` that is not valid UTF-8 is not a
    // merchant-presented code.
    let Ok(code) = str::from_utf8(code) else {
        return NOT_PAYMENT_CODE;
    };

    // 4.6.1.1 The Payload Format Indicator (ID "00") shall be the first data
    // object in the QR Code.
    let Some((first_data_object, rest)) = parse_next_data_object(code) else {
        return NOT_PAYMENT_CODE;
    };
    if first_data_object.id != PAYLOAD_FORMAT_INDICATOR_DATA_OBJECT_ID {
        return NOT_PAYMENT_CODE;
    }

    PaymentQrCodeValidation {
        scheme: identify_scheme(rest),
        result: validate_data_objects(code, rest),
    }
}

/// Returns the scheme of the first merchant account information template in
/// `data_objects` with a supported identifier, skipping malformed templates and
/// stopping at the first data object that does not parse.
fn identify_scheme(data_objects: &str) -> Option<PaymentQrCodeScheme> {
    let mut rest = data_objects;
    while let Some((data_object, next_rest)) = parse_next_data_object(rest) {
        if is_template_merchant_account_information(data_object.id)
            && let Ok(Some(identifier)) = get_template_identifier(&data_object)
            && let Some(scheme) = get_scheme_from_identifier(identifier)
        {
            return Some(scheme);
        }
        rest = next_rest;
    }
    None
}

/// Validates the root `data_objects` of `code`, after its payload format
/// indicator.
fn validate_data_objects(code: &str, data_objects: &str) -> PaymentQrCodeValidationResult {
    type ValidationResult = PaymentQrCodeValidationResult;

    // The CRC is verified before the content, so that a corrupted code is
    // reported as such rather than by whichever check its corruption breaks.
    let crc_data_object = match get_final_crc_data_object(data_objects) {
        Ok(crc_data_object) => crc_data_object,
        Err(result) => return result,
    };
    if !has_valid_crc(code, crc_data_object.value) {
        return ValidationResult::FailedCrcCheck;
    }

    let mut valid_result = None;
    let mut rest = data_objects;
    loop {
        let Some((data_object, next_rest)) = parse_next_data_object(rest) else {
            return ValidationResult::FailedTlvParsing;
        };
        match data_object.id {
            CRC16_DATA_OBJECT_ID => return valid_result.unwrap_or(ValidationResult::UnsupportedScheme),
            ADDITIONAL_DATA_FIELD_TEMPLATE_DATA_OBJECT_ID
                // 4.8.1.1: If present, the Additional Data Field Template shall contain at
                // least 1 data object.
                if !contains_valid_data_objects(data_object.value) =>
            {
                return ValidationResult::EmptyAdditionalDataFieldTemplate;
            }
            id if valid_result.is_none() && is_template_merchant_account_information(id) => {
                match validate_merchant_account_information(data_object) {
                    Ok(result) => valid_result = result,
                    Err(result) => return result,
                }
            }
            _ => (),
        }
        rest = next_rest;
    }
}

/// Returns the CRC data object of the root data objects in `data_objects`, or
/// an error if they do not parse or the CRC data object is not the final one.
fn get_final_crc_data_object(
    data_objects: &str,
) -> Result<Section<'_>, PaymentQrCodeValidationResult> {
    let mut rest = data_objects;
    loop {
        let Some((data_object, next_rest)) = parse_next_data_object(rest) else {
            return Err(PaymentQrCodeValidationResult::FailedTlvParsing);
        };
        if data_object.id != CRC16_DATA_OBJECT_ID {
            rest = next_rest;
            continue;
        }
        // 4.6.1.2 The CRC (ID "63") shall be the last data object in the QR
        // Code.
        if !next_rest.is_empty() {
            return Err(PaymentQrCodeValidationResult::FailedTlvParsing);
        }
        return Ok(data_object);
    }
}

fn is_template_merchant_account_information(id: &str) -> bool {
    id.parse::<u8>().is_ok_and(|id| {
        (FIRST_TEMPLATE_MERCHANT_ACCOUNT_INFORMATION_ID
            ..=LAST_TEMPLATE_MERCHANT_ACCOUNT_INFORMATION_ID)
            .contains(&id)
    })
}

/// Returns the Globally Unique Identifier of the merchant account information
/// `data_object`, `None` if it has none, or an error if the template is
/// malformed.
fn get_template_identifier<'a>(
    data_object: &Section<'a>,
) -> Result<Option<&'a str>, PaymentQrCodeValidationResult> {
    // 4.7.11.2: A Merchant Account Information template shall contain a
    // primitive Globally Unique Identifier data object with ID "00".
    let Some((identifier, _rest)) = parse_next_data_object(data_object.value) else {
        return Err(PaymentQrCodeValidationResult::FailedTlvParsing);
    };
    if identifier.id != GLOBALLY_UNIQUE_IDENTIFIER_DATA_OBJECT_ID {
        return Ok(None);
    }
    Ok(Some(identifier.value))
}

/// Returns the scheme of a merchant account information template with the
/// Globally Unique `identifier`, or `None` if it is not supported.
fn get_scheme_from_identifier(identifier: &str) -> Option<PaymentQrCodeScheme> {
    if identifier.eq_ignore_ascii_case(PIX_GLOBALLY_UNIQUE_IDENTIFIER) {
        return Some(PaymentQrCodeScheme::Pix);
    }
    if identifier.eq_ignore_ascii_case(DUITNOW_APPLICATION_IDENTIFIER) {
        return Some(PaymentQrCodeScheme::DuitNow);
    }
    if identifier.eq_ignore_ascii_case(PROMPTPAY_CREDIT_TRANSFER_APPLICATION_IDENTIFIER)
        || identifier.eq_ignore_ascii_case(PROMPTPAY_BILL_PAYMENT_APPLICATION_IDENTIFIER)
    {
        return Some(PaymentQrCodeScheme::PromptPay);
    }
    if identifier
        .get(..QRIS_GLOBALLY_UNIQUE_IDENTIFIER_PREFIX.len())
        .is_some_and(|prefix| prefix.eq_ignore_ascii_case(QRIS_GLOBALLY_UNIQUE_IDENTIFIER_PREFIX))
    {
        return Some(PaymentQrCodeScheme::Qris);
    }
    if identifier.eq_ignore_ascii_case(SHOPEEPAY_GLOBALLY_UNIQUE_IDENTIFIER) {
        return Some(PaymentQrCodeScheme::ShopeePay);
    }
    if identifier.eq_ignore_ascii_case(MOMO_GLOBALLY_UNIQUE_IDENTIFIER) {
        return Some(PaymentQrCodeScheme::MoMo);
    }
    None
}

/// Returns the valid result of the merchant account information `data_object`,
/// `None` if its identifier is missing or unsupported, so that another template
/// can still identify the scheme, or an error if the template is malformed.
fn validate_merchant_account_information(
    data_object: Section,
) -> Result<Option<PaymentQrCodeValidationResult>, PaymentQrCodeValidationResult> {
    type ValidationResult = PaymentQrCodeValidationResult;

    let Some(identifier) = get_template_identifier(&data_object)? else {
        return Ok(None);
    };
    match get_scheme_from_identifier(identifier) {
        None => Ok(None),
        Some(PaymentQrCodeScheme::Pix) => {
            match get_type_from_merchant_account_info_data_object(data_object) {
                Ok(PixQrCodeType::Static) => Ok(Some(ValidationResult::ValidPixStatic)),
                Ok(PixQrCodeType::Dynamic) => Ok(Some(ValidationResult::ValidPixDynamic)),
                Err(PixQrCodeError::UnknownPixCodeType) => Err(ValidationResult::UnsupportedScheme),
                Err(_) => Err(ValidationResult::FailedTlvParsing),
            }
        }
        Some(_) => Ok(Some(ValidationResult::Valid)),
    }
}

/// Returns whether `crc_value`, the value of the final CRC data object of
/// `code`, matches the CRC computed over the rest of `code`.
fn has_valid_crc(code: &str, crc_value: &str) -> bool {
    if crc_value.len() != CRC16_VALUE_LENGTH {
        return false;
    }
    // The CRC data object is final, so its value is the last characters.
    let crc_input = &code.as_bytes()[..code.len() - CRC16_VALUE_LENGTH];
    format!("{:04X}", compute_crc16(crc_input)).eq_ignore_ascii_case(crc_value)
}

fn compute_crc16(data: &[u8]) -> u16 {
    let mut crc = CRC16_INITIAL_VALUE;
    for &byte in data {
        crc ^= u16::from(byte) << BITS_PER_BYTE;
        for _ in 0..BITS_PER_BYTE {
            crc = if crc & CRC16_HIGH_BIT != 0 { (crc << 1) ^ CRC16_POLYNOMIAL } else { crc << 1 };
        }
    }
    crc
}
