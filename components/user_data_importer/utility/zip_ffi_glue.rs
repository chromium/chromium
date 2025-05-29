// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use anyhow::Result;
use cxx::CxxString;
use std::fs;
use std::io::Read;
use std::path::Path;
use std::pin::Pin;

#[cxx::bridge(namespace = "user_data_importer")]
mod ffi {
    enum FileType {
        Bookmarks,
        History,
        Passwords,
        PaymentCards,
    }

    extern "Rust" {
        fn unzip_using_rust(
            zip_filename: &[u8],
            file_to_unzip: FileType,
            output_bytes: Pin<&mut CxxString>,
        ) -> bool;
    }
}

// Verifies if the file in the provided path has the desired extension.
fn has_extension(path: &Path, expected_extension: &str) -> bool {
    path.extension()
        .map_or(false, |actual_extension| actual_extension.eq_ignore_ascii_case(expected_extension))
}

// Verifies if the file in the provided path contains history data.
fn has_history(path: &Path) -> bool {
    if !has_extension(&path, "json") {
        return false;
    }

    // TODO(crbug.com/407587751): Add JSON parser to this file to read the header.
    match path.to_str() {
        Some(path_str) => path_str == "History.json",
        None => false,
    }
}

// Verifies if the file in the provided path contains payment cards data.
fn has_payment_cards(path: &Path) -> bool {
    if !has_extension(&path, "json") {
        return false;
    }

    // TODO(crbug.com/407587751): Add JSON parser to this file to read the header.
    match path.to_str() {
        Some(path_str) => path_str == "PaymentCards.json",
        None => false,
    }
}

// Verifies if the file in the provided path contains the desired data type.
fn matches_requested_type(path: &Path, file_to_unzip: ffi::FileType) -> bool {
    match file_to_unzip {
        ffi::FileType::Bookmarks => has_extension(&path, "html"),
        ffi::FileType::History => has_history(&path),
        ffi::FileType::Passwords => has_extension(&path, "csv"),
        ffi::FileType::PaymentCards => has_payment_cards(&path),
        _ => false,
    }
}

fn unzip(
    zip_filename: &[u8],
    file_to_unzip: ffi::FileType,
    mut output_bytes: Pin<&mut CxxString>,
) -> Result<()> {
    let path = str::from_utf8(zip_filename)?;
    let file = fs::File::open(path)?;
    let mut archive = zip::ZipArchive::new(file)?;

    for i in 0..archive.len() {
        let Ok(mut file) = archive.by_index(i) else {
            continue;
        };
        let Some(outpath) = file.enclosed_name() else {
            continue;
        };

        // Read the first file matching the requested type found within the zip file.
        if matches_requested_type(&outpath.as_path(), file_to_unzip) {
            let mut file_contents = String::new();
            file.read_to_string(&mut file_contents)?;

            // Copy the contents of the file to the output.
            if file_contents.len() > 0 {
                output_bytes.as_mut().reserve(file_contents.len());
                output_bytes.as_mut().push_str(&file_contents);
            }
            break;
        }
    }

    Ok(())
}

pub fn unzip_using_rust(
    zip_filename: &[u8],
    file_to_unzip: ffi::FileType,
    output_bytes: Pin<&mut CxxString>,
) -> bool {
    unzip(zip_filename, file_to_unzip, output_bytes).is_ok()
}
