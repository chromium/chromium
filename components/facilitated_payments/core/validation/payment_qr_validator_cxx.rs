// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//components/facilitated_payments/core/validation:payment_qr_validator_rs";
}

#[cxx::bridge(namespace=payments::facilitated)]
pub mod ffi {
    /// It is not currently possible to return a `Result<T, E>` with cxx, so the
    /// success and error codes are flattened into a single enum.
    enum PixQrCodeResult {
        /// The input was successfully parsed as a dynamic Pix code.
        Dynamic,
        /// The input was successfully parsed as a static Pix code.
        Static,

        /// The input is probably not an EMV Merchant-Presented QR code; it did
        /// not begin with a valid data object.
        NotPaymentCode,
        /// The input contains at least one data object, but the first data
        /// object is not the required payload format indicator.
        MissingPayloadFormatIndicator,
        /// Distinct from `NonPixMerchantPresentedCode`; in this case, the input
        /// begins with one or more valid data objects, but a subsequent
        /// section did not parse as a valid data object.
        InvalidMerchantPresentedCode,
        /// The merchant account information data object does not begin with the
        /// required globally unique identifier.
        MissingGloballyUniqueIdentifier,
        /// The input appears to be a EMV Merchant-Presented QR code, but the
        /// globally unique identifier is not the Pix identifier.
        NonPixMerchantPresentedCode,
        /// The additional data field template data object is present but empty,
        /// which violates the EMV Merchant-Presented QR code
        /// requirements.
        EmptyAdditionalDataFieldTemplate,
        /// The CRC data object was not the final data object in the input.
        NonFinalCrc,
        /// The input is a Pix code, but of unknown type.
        UnknownPixCodeType,
    }

    /// The scheme of a payment QR code, identified even if the code fails
    /// validation.
    enum PaymentQrCodeScheme {
        /// No merchant account information template has a supported
        /// identifier, or the input is not an EMV Merchant-Presented QR code.
        Unknown,
        /// Pix (Brazil).
        Pix,
        /// DuitNow (Malaysia).
        DuitNow,
        /// PromptPay (Thailand).
        PromptPay,
        /// QRIS (Indonesia).
        Qris,
        /// ShopeePay (Malaysia).
        ShopeePay,
        /// MoMo (Vietnam).
        MoMo,
    }

    /// It is not currently possible to return a `Result<T, E>` with cxx, so the
    /// success and error codes are flattened into a single enum.
    enum PaymentQrCodeValidationResult {
        /// A valid code of a scheme other than Pix.
        Valid,
        /// A valid static Pix code.
        ValidPixStatic,
        /// A valid dynamic Pix code.
        ValidPixDynamic,

        /// The input is not an EMV Merchant-Presented QR code: it does not
        /// begin with the payload format indicator data object.
        NotPaymentCode,
        /// The input is a valid EMV Merchant-Presented QR code, but no merchant
        /// account information template identifies a supported scheme, or its
        /// Pix template is of an unknown type.
        UnsupportedScheme,
        /// A data object or template did not parse, or the CRC data object is
        /// missing or not the final data object.
        FailedTlvParsing,
        /// The CRC does not match the rest of the input.
        FailedCrcCheck,
        /// The additional data field template data object is present but empty.
        EmptyAdditionalDataFieldTemplate,
    }

    /// The scheme and the validation result are separate, so that failures
    /// can be measured per scheme.
    struct PaymentQrCodeValidation {
        scheme: PaymentQrCodeScheme,
        result: PaymentQrCodeValidationResult,
    }

    extern "Rust" {
        fn get_pix_qr_code_type(code: &[u8]) -> PixQrCodeResult;
        fn validate_payment_qr_code(code: &[u8]) -> PaymentQrCodeValidation;
    }
}

impl std::convert::From<payment_qr_validator_rs::PixQrCodeResult> for ffi::PixQrCodeResult {
    fn from(value: payment_qr_validator_rs::PixQrCodeResult) -> Self {
        match value {
            Ok(payment_qr_validator_rs::PixQrCodeType::Dynamic) => Self::Dynamic,
            Ok(payment_qr_validator_rs::PixQrCodeType::Static) => Self::Static,
            Err(payment_qr_validator_rs::PixQrCodeError::NotPaymentCode) => Self::NotPaymentCode,
            Err(payment_qr_validator_rs::PixQrCodeError::MissingPayloadFormatIndicator) => {
                Self::MissingPayloadFormatIndicator
            }
            Err(payment_qr_validator_rs::PixQrCodeError::InvalidMerchantPresentedCode) => {
                Self::InvalidMerchantPresentedCode
            }
            Err(payment_qr_validator_rs::PixQrCodeError::MissingGloballyUniqueIdentifier) => {
                Self::MissingGloballyUniqueIdentifier
            }
            Err(payment_qr_validator_rs::PixQrCodeError::NonPixMerchantPresentedCode) => {
                Self::NonPixMerchantPresentedCode
            }
            Err(payment_qr_validator_rs::PixQrCodeError::EmptyAdditionalDataFieldTemplate) => {
                Self::EmptyAdditionalDataFieldTemplate
            }
            Err(payment_qr_validator_rs::PixQrCodeError::NonFinalCrc) => Self::NonFinalCrc,
            Err(payment_qr_validator_rs::PixQrCodeError::UnknownPixCodeType) => {
                Self::UnknownPixCodeType
            }
        }
    }
}

fn get_pix_qr_code_type(code: &[u8]) -> ffi::PixQrCodeResult {
    payment_qr_validator_rs::get_pix_qr_code_type(code).into()
}

impl std::convert::From<payment_qr_validator_rs::PaymentQrCodeValidation>
    for ffi::PaymentQrCodeValidation
{
    fn from(value: payment_qr_validator_rs::PaymentQrCodeValidation) -> Self {
        use payment_qr_validator_rs::{PaymentQrCodeScheme, PaymentQrCodeValidationResult};
        let scheme = match value.scheme {
            None => ffi::PaymentQrCodeScheme::Unknown,
            Some(PaymentQrCodeScheme::Pix) => ffi::PaymentQrCodeScheme::Pix,
            Some(PaymentQrCodeScheme::DuitNow) => ffi::PaymentQrCodeScheme::DuitNow,
            Some(PaymentQrCodeScheme::PromptPay) => ffi::PaymentQrCodeScheme::PromptPay,
            Some(PaymentQrCodeScheme::Qris) => ffi::PaymentQrCodeScheme::Qris,
            Some(PaymentQrCodeScheme::ShopeePay) => ffi::PaymentQrCodeScheme::ShopeePay,
            Some(PaymentQrCodeScheme::MoMo) => ffi::PaymentQrCodeScheme::MoMo,
        };
        let result = match value.result {
            PaymentQrCodeValidationResult::Valid => ffi::PaymentQrCodeValidationResult::Valid,
            PaymentQrCodeValidationResult::ValidPixStatic => {
                ffi::PaymentQrCodeValidationResult::ValidPixStatic
            }
            PaymentQrCodeValidationResult::ValidPixDynamic => {
                ffi::PaymentQrCodeValidationResult::ValidPixDynamic
            }
            PaymentQrCodeValidationResult::NotPaymentCode => {
                ffi::PaymentQrCodeValidationResult::NotPaymentCode
            }
            PaymentQrCodeValidationResult::UnsupportedScheme => {
                ffi::PaymentQrCodeValidationResult::UnsupportedScheme
            }
            PaymentQrCodeValidationResult::FailedTlvParsing => {
                ffi::PaymentQrCodeValidationResult::FailedTlvParsing
            }
            PaymentQrCodeValidationResult::FailedCrcCheck => {
                ffi::PaymentQrCodeValidationResult::FailedCrcCheck
            }
            PaymentQrCodeValidationResult::EmptyAdditionalDataFieldTemplate => {
                ffi::PaymentQrCodeValidationResult::EmptyAdditionalDataFieldTemplate
            }
        };
        Self { scheme, result }
    }
}

fn validate_payment_qr_code(code: &[u8]) -> ffi::PaymentQrCodeValidation {
    payment_qr_validator_rs::validate_payment_qr_code(code).into()
}
