// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod history;
mod json;
mod models;
mod utils;
mod zip_archive;

use crate::zip_archive::{ResultOfZipFileArchive, ZipFileArchive};

#[cxx::bridge(namespace = "user_data_importer")]
mod ffi {
    enum FileType {
        Bookmarks,
        Passwords,
        PaymentCards,
        SafariHistory,
        StablePortabilityHistory,
    }

    // C++ interop version of the SafariHistoryJSONEntry structure.
    // See SafariHistoryJSONEntry for field documentation.
    struct SafariHistoryEntry {
        url: String,
        title: String,
        time_usec: u64,
        destination_url: String,
        source_url: String,
        visit_count: u64,
    }

    // C++ interop version of the StablePortabilityHistoryJSONEntry structure.
    // See StablePortabilityHistoryJSONEntry for field documentation.
    struct StablePortabilityHistoryEntry {
        synced: bool,
        title: String,
        url: String,
        visit_time_unix_epoch_usec: u64,
        visit_count: u64,
        typed_count: u64,
    }

    // C++ interop version of the PaymentCardJSONEntry structure.
    // See PaymentCardJSONEntry for field documentation.
    struct PaymentCardEntry {
        card_number: String,
        card_name: String,
        cardholder_name: String,
        card_expiration_month: u64,
        card_expiration_year: u64,
    }

    unsafe extern "C++" {
        include!("components/user_data_importer/utility/history_callback_from_rust.h");

        type SafariHistoryCallbackFromRust;
        #[cxx_name = "ImportHistoryEntries"]
        fn ImportSafariHistoryEntries(
            self: Pin<&mut SafariHistoryCallbackFromRust>,
            history_entries: UniquePtr<CxxVector<SafariHistoryEntry>>,
            completed: bool,
        );

        type StablePortabilityHistoryCallbackFromRust;
        #[cxx_name = "ImportHistoryEntries"]
        fn ImportStablePortabilityHistoryEntries(
            self: Pin<&mut StablePortabilityHistoryCallbackFromRust>,
            history_entries: UniquePtr<CxxVector<StablePortabilityHistoryEntry>>,
            completed: bool,
        );
    }

    extern "Rust" {
        type ResultOfZipFileArchive;
        fn err(self: &ResultOfZipFileArchive) -> bool;
        fn unwrap(self: &mut ResultOfZipFileArchive) -> Box<ZipFileArchive>;

        type ZipFileArchive;
        fn get_file_size_bytes(self: &mut ZipFileArchive, file_type: FileType) -> u64;
        fn unzip(
            self: &mut ZipFileArchive,
            file_type: FileType,
            mut output_bytes: Pin<&mut CxxString>,
        ) -> bool;
        fn parse_safari_history(
            self: &mut ZipFileArchive,
            history_callback: UniquePtr<SafariHistoryCallbackFromRust>,
            history_size_threshold: usize,
        );
        fn parse_payment_cards(
            self: &mut ZipFileArchive,
            history: Pin<&mut CxxVector<PaymentCardEntry>>,
        ) -> bool;

        fn new_archive(zip_filename: &[u8]) -> Box<ResultOfZipFileArchive>;
        fn parse_stable_portability_history(
            json_filename: &[u8],
            history_callback: UniquePtr<StablePortabilityHistoryCallbackFromRust>,
            history_size_threshold: usize,
        ) -> bool;
    }
}

// Attempts to parse a file in the stable portability data format. Returns true if the file was
// successfully parsed, false otherwise.
fn parse_stable_portability_history(
    json_filename: &[u8],
    history_callback: cxx::UniquePtr<ffi::StablePortabilityHistoryCallbackFromRust>,
    history_size_threshold: usize,
) -> bool {
    history::parse_stable_portability_history(
        json_filename,
        history_callback,
        history_size_threshold,
    )
}

fn new_archive(zip_filename: &[u8]) -> Box<ResultOfZipFileArchive> {
    zip_archive::new_archive(zip_filename)
}