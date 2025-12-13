// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ffi;
use crate::json::{self, ZipEntryBufReader};
use crate::models::SafariHistoryJSONEntry;
use crate::{utils::has_extension, ZipFileArchive};
use cxx::{CxxVector, UniquePtr};
use std::io::Read;
use std::mem;
use std::pin::Pin;
use zip;

#[cfg(target_family = "unix")]
use crate::json::STREAM_BUFFER_SIZE;
#[cfg(target_family = "unix")]
use crate::models::StablePortabilityHistoryJSONEntry;
#[cfg(target_family = "unix")]
use std::fs;
#[cfg(target_family = "unix")]
use std::io::BufReader;

// A trait for history callbacks to allow for generic implementation of
// batching.
pub trait HistoryCallback<T: cxx::vector::VectorElement> {
    fn import_entries(self: Pin<&mut Self>, entries: UniquePtr<CxxVector<T>>, completed: bool);
    fn fail(self: Pin<&mut Self>);
}

impl HistoryCallback<ffi::SafariHistoryEntry> for ffi::SafariHistoryCallbackFromRust {
    fn import_entries(
        self: Pin<&mut Self>,
        entries: UniquePtr<CxxVector<ffi::SafariHistoryEntry>>,
        completed: bool,
    ) {
        self.ImportSafariHistoryEntries(entries, completed);
    }

    fn fail(self: Pin<&mut Self>) {
        self.Fail();
    }
}

impl HistoryCallback<ffi::StablePortabilityHistoryEntry>
    for ffi::StablePortabilityHistoryCallbackFromRust
{
    fn import_entries(
        self: Pin<&mut Self>,
        entries: UniquePtr<CxxVector<ffi::StablePortabilityHistoryEntry>>,
        completed: bool,
    ) {
        self.ImportStablePortabilityHistoryEntries(entries, completed);
    }

    fn fail(self: Pin<&mut Self>) {
        self.Fail();
    }
}

// Adds an item to a history batch, sending a batch if the threshold is reached.
//
// Type Parameters:
//
// - `T`: The type of the history item to be added to the batch. For example,
//   `SafariHistoryJSONEntry` or `StablePortabilityHistoryJSONEntry`.
// - `U`: The type of the element stored in the C++ vector. This is the same as
//   `T` but with the `cxx::ExternType` trait.
// - `C`: The type of the callback object, which must implement
//   `HistoryCallback<U>`.
//
// Arguments:
//
// - `item`: The history item to add to the batch.
// - `history`: The vector where history items are accumulated.
// - `threshold`: The size at which the batch is sent to the callback.
// - `callback`: The C++ callback to invoke with the batch of history items.
pub fn batch_and_send<T, U, C>(
    item: T,
    history: &mut UniquePtr<CxxVector<U>>,
    threshold: usize,
    callback: Pin<&mut C>,
) where
    T: Into<U>,
    U: 'static + cxx::vector::VectorElement + cxx::ExternType<Kind = cxx::kind::Trivial>,
    C: HistoryCallback<U> + ?Sized,
{
    history.as_mut().unwrap().push(item.into());
    if history.len() >= threshold {
        let batch_to_send = mem::replace(history, CxxVector::<U>::new());
        callback.import_entries(batch_to_send, /* completed= */ false);
    }
}

// Attempts to parse a file in the Safari history format.
pub fn parse_safari_history(
    archive: &mut ZipFileArchive,
    mut history_callback: UniquePtr<ffi::SafariHistoryCallbackFromRust>,
    history_size_threshold: usize,
) {
    let mut history = CxxVector::<ffi::SafariHistoryEntry>::new();
    let result = archive.fold_files(true, |acc, file, outpath| {
        if has_extension(outpath, ffi::FileType::SafariHistory) {
            let stream_reader = ZipEntryBufReader::new(file);
            let result = json::deserialize_top_level::<SafariHistoryJSONEntry, _>(
                stream_reader.inner,
                ffi::FileType::SafariHistory,
                |history_item| {
                    batch_and_send(
                        history_item,
                        &mut history,
                        history_size_threshold,
                        history_callback.as_mut().unwrap(),
                    );
                },
                /* metadata_only= */ false,
            );

            match result {
                Ok(_) => acc,
                Err(e) => {
                    if e.contains("Unexpected data type") {
                        // This is not a real error, just the wrong data type (e.g., PaymentCards).
                        acc
                    } else {
                        false
                    }
                }
            }
        } else {
            acc
        }
    });

    if result {
        history_callback.as_mut().unwrap().import_entries(history, /* completed= */ true);
    } else {
        history_callback.as_mut().unwrap().fail();
    }
}

// Attempts to parse a file in the stable portability history format. Returns
// whether parsing was successful.
#[cfg(target_family = "unix")]
pub fn parse_stable_portability_history(
    file: fs::File,
    mut history_callback: UniquePtr<ffi::StablePortabilityHistoryCallbackFromRust>,
    history_size_threshold: usize,
) {
    let mut history = CxxVector::<ffi::StablePortabilityHistoryEntry>::new();
    let result = (|| -> Result<(), String> {
        let stream_reader = BufReader::with_capacity(STREAM_BUFFER_SIZE, file);
        json::deserialize_top_level::<StablePortabilityHistoryJSONEntry, std::fs::File>(
            stream_reader,
            ffi::FileType::StablePortabilityHistory,
            |history_item| {
                batch_and_send(
                    history_item,
                    &mut history,
                    history_size_threshold,
                    history_callback.as_mut().unwrap(),
                );
            },
            /* metadata_only= */ false,
        )
    })();
    if result.is_ok() {
        // Send final batch if any, and completion signal.
        history_callback.as_mut().unwrap().import_entries(history, true);
    } else {
        // Signal failure.
        history_callback.as_mut().unwrap().fail();
    }
}

// Returns whether the file used by the stream reader is a history file.
pub fn is_safari_history_file<'a, R: Read>(stream_reader: ZipEntryBufReader<'a, R>) -> bool {
    return json::deserialize_top_level::<SafariHistoryJSONEntry, zip::read::ZipFile<'a, R>>(
        stream_reader.inner,
        ffi::FileType::SafariHistory,
        |_| {},
        /* metadata_only= */ true,
    )
    .is_ok();
}
