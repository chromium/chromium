// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use cxx::CxxVector;
use qr_code::types::{Color, EcLevel, QrError};
use qr_code::{QrCode, Version};
use std::pin::Pin;

#[cxx::bridge(namespace = "qr_code_generator")]
mod ffi {
    #[repr(u8)]
    enum Error {
        kUnknownError = 0,
        kInputTooLong = 1,
    }

    extern "C++" {
        include!("components/qr_code_generator/error.h");
        type Error;
    }

    extern "Rust" {
        fn generate_qr_code_using_rust(
            data: &[u8],
            min_version: i16,
            out_pixels: Pin<&mut CxxVector<u8>>,
            out_qr_size: &mut usize,
            out_error: &mut Error,
        ) -> bool;
    }
}

/// Translates `data` into a QR code.
///
/// `min_version` can request a minimum QR code version.
fn generate(data: &[u8], min_version: Option<i16>) -> Result<QrCode, QrError> {
    let mut qr_code = QrCode::new(data)?;

    let actual_version = match qr_code.version() {
        Version::Micro(_) => panic!("QrCode::new should not generate micro QR codes"),
        Version::Normal(actual_version) => actual_version,
    };

    match min_version {
        None => (),
        Some(min_version) if actual_version >= min_version => (),
        Some(min_version) => {
            // If `actual_version` < `min_version`, then re-encode using `min_version`
            qr_code = QrCode::with_version(data, Version::Normal(min_version), EcLevel::M)?;
        }
    }

    Ok(qr_code)
}

/// Translates `data` into a QR code.
///
/// `min_version` can request a minimum QR code version (e.g. if the caller
/// requires that the QR code has a certain minimal width and height;  for
/// example, QR code version 5 translates into 37x37 QR pixels).  Setting
/// `min_version` to 0 means that the caller doesn't have any QR version
/// requirements.
///
/// On success returns `true` and populates `out_pixels` and `out_qr_size`.  On
/// failure returns `false` and populates `out_error`.
pub fn generate_qr_code_using_rust(
    data: &[u8],
    min_version: i16,
    mut out_pixels: Pin<&mut CxxVector<u8>>,
    out_qr_size: &mut usize,
    out_error: &mut ffi::Error,
) -> bool {
    let min_version = match min_version {
        0 => None,
        1..=40 => Some(min_version),
        _ => {
            *out_error = ffi::Error::kUnknownError;
            return false;
        }
    };

    match generate(data, min_version) {
        Err(err) => {
            *out_error = match err {
                QrError::DataTooLong => ffi::Error::kInputTooLong,
                _ => ffi::Error::kUnknownError,
            };
            false
        }
        Ok(qr_code) => {
            *out_qr_size = qr_code.width().into();

            assert!(out_pixels.is_empty());
            for color in qr_code.into_colors() {
                let u8_value = match color {
                    Color::Light => 0,
                    Color::Dark => 1,
                };
                out_pixels.as_mut().push(u8_value);
            }

            true
        }
    }
}
