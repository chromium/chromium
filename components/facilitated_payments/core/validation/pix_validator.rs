// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A Pix code uses the format defined in EMV Merchant-Presented QR codes:
// https://www.emvco.com/emv-technologies/qr-codes/

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
    //   defined in [EMV Book 4]. Interestingly enough, the Merchant-Presented QR
    //   code specification claims 96 characters, while EMV Book 4 only claims 95
    //   characters. The characters in question are the characters that are common
    //   to all ISO 8859 encodings, with codepoints from 0x20 to 0x7F.
    // - Unicode: encoded as UTF-8
    //
    // So any `code` that is not valid UTF-8 can be ignored.
    let code = match str::from_utf8(code) {
        Ok(code) => code,
        Err(_) => return Err(Error::NotPaymentCode),
    };

    // 4.6.1.1 The Payload Format Indicator (ID "00") shall be the first data object
    // in the QR Code.
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
            ADDITIONAL_DATA_FIELD_TEMPLATE_DATA_OBJECT_ID => {
                // 4.8.1.1: If present, the Additional Data Field Template shall contain at
                // least 1 data object.
                if !contains_valid_data_objects(next_data_object.value) {
                    return Err(Error::EmptyAdditionalDataFieldTemplate);
                }
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
    // - The length is coded as a two-digit numeric value, with a value ranging from
    //   "01" to "99"
    // - The value field has a minimum length of one character and maximum length of
    //   99 characters.
    let (id, rest) = input.split_at_checked(2)?;
    let (length, rest) = rest.split_at_checked(2)?;
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

    // 4.7.11.2: If present, a Merchant Account Information template shall contain a
    // primitive Globally Unique Identifier data object with a data object ID
    // "00", as defined in Table 4.2.
    let Some((gui_data_object, rest)) = parse_next_data_object(data_object.value) else {
        return Err(Error::InvalidMerchantPresentedCode);
    };
    if gui_data_object.id != "00" {
        return Err(Error::MissingGloballyUniqueIdentifier);
    }

    if !gui_data_object.value.eq_ignore_ascii_case(PIX_GLOBALLY_UNIQUE_IDENTIFIER) {
        return Err(Error::NonPixMerchantPresentedCode);
    }

    // 4.7.11.2: The value of the Globally Unique Identifier sets the context for
    // the remainder of the template and the meaning of the other data objects
    // in the template are context specific and outside of the scope of EMVCo.
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
    // While Pix does not appear to mandate a specific ordering for data objects, in
    // practice, the validator expects the first data object after the Globally
    // Unique Identifier to indicate whether the Pix code is static or dynamic.
    // If it does not, treat the Pix code as malformed.
    match next_data_object.id {
        MERCHANT_ACCOUNT_INFORMATION_DYNAMIC_URL_DATA_OBJECT_ID => Ok(PixQrCodeType::Dynamic),
        MERCHANT_ACCOUNT_INFORMATION_STATIC_KEY_DATA_OBJECT_ID => Ok(PixQrCodeType::Static),
        _ => Err(Error::UnknownPixCodeType),
    }
}
