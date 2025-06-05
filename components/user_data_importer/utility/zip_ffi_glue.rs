// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use anyhow::{anyhow, Error, Result};
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
        type ResultOfZipFileArchive;
        fn err(self: &ResultOfZipFileArchive) -> bool;
        fn unwrap(self: &mut ResultOfZipFileArchive) -> Box<ZipFileArchive>;

        type ZipFileArchive;
        fn get_file_size(self: &mut ZipFileArchive, file_to_unzip: FileType) -> u64;
        fn unzip(
            self: &mut ZipFileArchive,
            file_to_unzip: FileType,
            mut output_bytes: Pin<&mut CxxString>,
        ) -> bool;

        fn new_archive(zip_filename: &[u8]) -> Box<ResultOfZipFileArchive>;
    }
}

/// FFI-friendly wrapper around `Result<T, E>` (`cxx` can't handle arbitrary
/// generics, so we manually monomorphize here, but still expose a minimal,
/// somewhat tweaked API of the original type).
pub struct ResultOfZipFileArchive(Result<ZipFileArchive, Error>);

impl ResultOfZipFileArchive {
    fn err(&self) -> bool {
        self.0.as_ref().is_err()
    }

    fn unwrap(&mut self) -> Box<ZipFileArchive> {
        // Leaving `self` in a C++-friendly "moved-away" state.
        let mut result = Err(anyhow!("Failed to get archive!"));
        std::mem::swap(&mut self.0, &mut result);
        Box::new(result.unwrap())
    }
}

fn create_archive(zip_filename: &[u8]) -> Result<ZipFileArchive> {
    let path = str::from_utf8(zip_filename)?;
    let file = fs::File::open(path)?;
    let archive = zip::ZipArchive::new(file)?;
    Ok(ZipFileArchive { archive: archive })
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

pub fn new_archive(zip_filename: &[u8]) -> Box<ResultOfZipFileArchive> {
    Box::new(ResultOfZipFileArchive(create_archive(zip_filename)))
}

/// FFI-friendly wrapper around `zip::ZipArchive` (`cxx` can't handle arbitrary
/// generics, so we manually monomorphize here, but still expose a minimal,
/// somewhat tweaked API of the original type).
struct ZipFileArchive {
    archive: zip::ZipArchive<std::fs::File>,
}

impl ZipFileArchive {
    fn get_file_size(&mut self, file_to_unzip: ffi::FileType) -> u64 {
        for i in 0..self.archive.len() {
            let Ok(file) = self.archive.by_index(i) else {
                continue;
            };
            let Some(outpath) = file.enclosed_name() else {
                continue;
            };

            // Read the first file matching the requested type found within the zip file.
            if matches_requested_type(&outpath.as_path(), file_to_unzip) {
                return file.size();
            }
        }

        return 0;
    }

    fn unzip(
        self: &mut ZipFileArchive,
        file_to_unzip: ffi::FileType,
        mut output_bytes: Pin<&mut CxxString>,
    ) -> bool {
        for i in 0..self.archive.len() {
            let Ok(mut file) = self.archive.by_index(i) else {
                continue;
            };
            let Some(outpath) = file.enclosed_name() else {
                continue;
            };

            // Read the first file matching the requested type found within the zip file.
            if matches_requested_type(&outpath.as_path(), file_to_unzip) {
                let mut file_contents = String::new();
                let Ok(_) = file.read_to_string(&mut file_contents) else {
                    return false;
                };

                // Copy the contents of the file to the output.
                if file_contents.len() > 0 {
                    output_bytes.as_mut().reserve(file_contents.len());
                    output_bytes.as_mut().push_str(&file_contents);
                }
                return true;
            }
        }

        return false;
    }
}
