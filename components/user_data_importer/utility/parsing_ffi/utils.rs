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

// Returns whether the file should be processed for the provided file type.
// This checks if the file has the correct extension and is not a hidden file.
pub(crate) fn should_process_file(path: &Path, file_type: ffi::FileType) -> bool {
    let Ok(ext) = expected_extension(file_type) else {
        return false;
    };

    if path.file_name().is_some_and(|name| name.to_string_lossy().starts_with(".")) {
        return false;
    }

    path.extension().is_some_and(|actual_extension| actual_extension.eq_ignore_ascii_case(ext))
}
