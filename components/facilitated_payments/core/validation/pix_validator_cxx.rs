// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//components/facilitated_payments/core/validation:pix_validator_rs";
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

    extern "Rust" {
        fn get_pix_qr_code_type(code: &[u8]) -> PixQrCodeResult;
    }
}

impl std::convert::From<pix_validator_rs::PixQrCodeResult> for ffi::PixQrCodeResult {
    fn from(value: pix_validator_rs::PixQrCodeResult) -> Self {
        match value {
            Ok(pix_validator_rs::PixQrCodeType::Dynamic) => Self::Dynamic,
            Ok(pix_validator_rs::PixQrCodeType::Static) => Self::Static,
            Err(pix_validator_rs::PixQrCodeError::NotPaymentCode) => Self::NotPaymentCode,
            Err(pix_validator_rs::PixQrCodeError::MissingPayloadFormatIndicator) => {
                Self::MissingPayloadFormatIndicator
            }
            Err(pix_validator_rs::PixQrCodeError::InvalidMerchantPresentedCode) => {
                Self::InvalidMerchantPresentedCode
            }
            Err(pix_validator_rs::PixQrCodeError::MissingGloballyUniqueIdentifier) => {
                Self::MissingGloballyUniqueIdentifier
            }
            Err(pix_validator_rs::PixQrCodeError::NonPixMerchantPresentedCode) => {
                Self::NonPixMerchantPresentedCode
            }
            Err(pix_validator_rs::PixQrCodeError::EmptyAdditionalDataFieldTemplate) => {
                Self::EmptyAdditionalDataFieldTemplate
            }
            Err(pix_validator_rs::PixQrCodeError::NonFinalCrc) => Self::NonFinalCrc,
            Err(pix_validator_rs::PixQrCodeError::UnknownPixCodeType) => Self::UnknownPixCodeType,
        }
    }
}

fn get_pix_qr_code_type(code: &[u8]) -> ffi::PixQrCodeResult {
    pix_validator_rs::get_pix_qr_code_type(code).into()
}
