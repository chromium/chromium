// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//components/facilitated_payments/core/validation:pix_validator_rs";
}

#[cxx::bridge(namespace=payments::facilitated)]
pub mod ffi {
    enum PixQrCodeType {
        Invalid,
        Dynamic,
        Static,
    }

    extern "Rust" {
        fn get_pix_qr_code_type(code: &[u8]) -> PixQrCodeType;
    }
}

impl std::convert::From<Option<pix_validator_rs::PixQrCodeType>> for ffi::PixQrCodeType {
    fn from(value: Option<pix_validator_rs::PixQrCodeType>) -> Self {
        match value {
            None => Self::Invalid,
            Some(pix_validator_rs::PixQrCodeType::Dynamic) => Self::Dynamic,
            Some(pix_validator_rs::PixQrCodeType::Static) => Self::Static,
        }
    }
}

fn get_pix_qr_code_type(code: &[u8]) -> ffi::PixQrCodeType {
    pix_validator_rs::get_pix_qr_code_type(code).into()
}
