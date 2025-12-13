// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ffi;
use anyhow::{anyhow, Result};
use std::path::Path;

// Returns the expected extension for the provided file type.
pub(crate) fn expected_extension(file_type: ffi::FileType) -> Result<&'static str> {
    match file_type {
        ffi::FileType::Bookmarks => Ok("html"),
        ffi::FileType::SafariHistory => Ok("json"),
        ffi::FileType::Passwords => Ok("csv"),
        ffi::FileType::PaymentCards => Ok("json"),
        _ => Err(anyhow!("Unknown file type")),
    }
}

// Verifies if the file in the provided path has the desired extension.
pub(crate) fn has_extension(path: &Path, file_type: ffi::FileType) -> bool {
    let Ok(ext) = expected_extension(file_type) else {
        return false;
    };

    path.extension().map_or(false, |actual_extension| actual_extension.eq_ignore_ascii_case(ext))
}
