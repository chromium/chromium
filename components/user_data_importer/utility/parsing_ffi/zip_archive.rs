// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ffi;
use crate::history;
use crate::json::{self, ZipEntryBufReader};
use crate::models::PaymentCardJSONEntry;
use crate::utils::has_extension;

use anyhow::{anyhow, Error, Result};
use cxx::{CxxString, CxxVector};
use std::fs;
use std::io::Read;
use std::path::Path;
use std::pin::Pin;
use std::str;

// Attempts to parse the payment cards file. Returns whether parsing was
// successful.
fn parse_payment_cards_file<'a, R: Read>(
    stream_reader: ZipEntryBufReader<'a, R>,
    callback: impl FnMut(PaymentCardJSONEntry) + 'a,
) -> bool {
    return json::deserialize_top_level(
        stream_reader.inner,
        ffi::FileType::PaymentCards,
        callback,
        /* metadata_only= */ false,
    )
    .is_ok();
}

// Returns whether the file used by the stream reader is a payment cards file.
fn is_payment_cards_file<'a, R: Read>(stream_reader: ZipEntryBufReader<'a, R>) -> bool {
    return json::deserialize_top_level::<PaymentCardJSONEntry, zip::read::ZipFile<'a, R>>(
        stream_reader.inner,
        ffi::FileType::PaymentCards,
        |_| {},
        /* metadata_only= */ true,
    )
    .is_ok();
}

/// FFI-friendly wrapper around `Result<T, E>` (`cxx` can't handle arbitrary
/// generics, so we manually monomorphize here, but still expose a minimal,
/// somewhat tweaked API of the original type).
pub struct ResultOfZipFileArchive(Result<ZipFileArchive, Error>);

impl ResultOfZipFileArchive {
    pub fn err(&self) -> bool {
        self.0.as_ref().is_err()
    }

    pub fn unwrap(&mut self) -> Box<ZipFileArchive> {
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

pub fn new_archive(zip_filename: &[u8]) -> Box<ResultOfZipFileArchive> {
    Box::new(ResultOfZipFileArchive(create_archive(zip_filename)))
}

/// FFI-friendly wrapper around `zip::ZipArchive` (`cxx` can't handle arbitrary
/// generics, so we manually monomorphize here, but still expose a minimal,
/// somewhat tweaked API of the original type).
pub struct ZipFileArchive {
    archive: zip::ZipArchive<std::fs::File>,
}

impl ZipFileArchive {
    // Iterates over files archived in `self` and return results of `f` for the
    // first file where `f` returns `Some` rather than `None`.
    fn find_and_map_file<F, R>(&mut self, mut f: F) -> Option<R>
    where
        F: FnMut(zip::read::ZipFile<std::fs::File>, &Path) -> Option<R>,
    {
        for i in 0..self.archive.len() {
            if let Ok(file) = self.archive.by_index(i) {
                if let Some(outpath) = file.enclosed_name() {
                    if let Some(r) = f(file, &outpath) {
                        return Some(r);
                    }
                }
            }
        }
        None
    }

    // Iterates over all files archived in `self` and accumulates a single result by
    // applying `f`.
    pub fn fold_files<F, B>(&mut self, init: B, mut f: F) -> B
    where
        F: FnMut(B, zip::read::ZipFile<std::fs::File>, &Path) -> B,
    {
        let mut acc = init;
        for i in 0..self.archive.len() {
            if let Ok(file) = self.archive.by_index(i) {
                if let Some(outpath) = file.enclosed_name() {
                    acc = f(acc, file, &outpath);
                }
            }
        }
        acc
    }

    pub fn get_file_size_bytes(&mut self, file_type: ffi::FileType) -> u64 {
        // Since there can be multiple history files, we need to sum the sizes of all of
        // them.
        if file_type == ffi::FileType::SafariHistory {
            return self.fold_files(0u64, |mut total_file_size_bytes, file, outpath| {
                if has_extension(outpath, file_type) {
                    let file_size_bytes = file.size();
                    let stream_reader = ZipEntryBufReader::new(file);
                    if history::is_safari_history_file(stream_reader) {
                        total_file_size_bytes += file_size_bytes;
                    }
                }
                total_file_size_bytes
            });
        }

        // All other types are find operations with a size check after file
        // selection.
        let size = self.find_and_map_file(|file, outpath| {
            if has_extension(outpath, file_type) {
                if file_type == ffi::FileType::Bookmarks || file_type == ffi::FileType::Passwords {
                    return Some(file.size());
                } else if file_type == ffi::FileType::PaymentCards {
                    let file_size = file.size();
                    if is_payment_cards_file(ZipEntryBufReader::new(file)) {
                        return Some(file_size);
                    }
                }
            }
            None
        });

        size.unwrap_or(0)
    }

    pub fn unzip(
        self: &mut ZipFileArchive,
        file_type: ffi::FileType,
        mut output_bytes: Pin<&mut CxxString>,
    ) -> bool {
        // Only Bookmarks and Passwords unzip the file as raw data. Other types use JSON
        // parsing.
        if file_type != ffi::FileType::Bookmarks && file_type != ffi::FileType::Passwords {
            return false;
        }

        let result = self.find_and_map_file(|mut file, outpath| {
            if has_extension(outpath, file_type) {
                // Read the first file matching the requested type found within the zip file.
                let mut file_contents = String::new();
                if file.read_to_string(&mut file_contents).is_err() {
                    return Some(false);
                };

                // Copy the contents of the file to the output.
                if file_contents.len() > 0 {
                    output_bytes.as_mut().reserve(file_contents.len());
                    output_bytes.as_mut().push_str(&file_contents);
                }
                return Some(true);
            }
            None
        });
        result.unwrap_or(false)
    }

    pub fn parse_safari_history(
        self: &mut ZipFileArchive,
        history_callback: cxx::UniquePtr<ffi::SafariHistoryCallbackFromRust>,
        history_size_threshold: usize,
    ) {
        history::parse_safari_history(self, history_callback, history_size_threshold);
    }

    pub fn parse_payment_cards(
        self: &mut ZipFileArchive,
        mut payment_cards: Pin<&mut CxxVector<ffi::PaymentCardEntry>>,
    ) -> bool {
        let result = self.find_and_map_file(|file, outpath| {
            if has_extension(outpath, ffi::FileType::PaymentCards) {
                let stream_reader = ZipEntryBufReader::new(file);
                if parse_payment_cards_file(stream_reader, |payment_card_item| {
                    payment_cards.as_mut().push(payment_card_item.into());
                }) {
                    return Some(true);
                }
            }
            None
        });
        result.unwrap_or(false)
    }
}
