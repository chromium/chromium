// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod models;

use crate::models::{
    Metadata, PaymentCardJSONEntry, SafariHistoryJSONEntry, StablePortabilityHistoryJSONEntry,
};

use anyhow::{anyhow, Error, Result};
use cxx::{CxxString, CxxVector};
use serde::{de, de::Deserializer, de::Error as DeserializerError, Deserialize};
use serde_json_lenient;
use std::fmt;
use std::fs;
use std::io::{BufReader, Read};
use std::path::Path;
use std::pin::Pin;

const STREAM_BUFFER_SIZE: usize = 4096;

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

// A trait for history callbacks to allow for generic implementation of
// batching.
trait HistoryCallback<T: cxx::vector::VectorElement> {
    fn import_entries(self: Pin<&mut Self>, entries: cxx::UniquePtr<CxxVector<T>>, completed: bool);
}

impl HistoryCallback<ffi::SafariHistoryEntry> for ffi::SafariHistoryCallbackFromRust {
    fn import_entries(
        self: Pin<&mut Self>,
        entries: cxx::UniquePtr<CxxVector<ffi::SafariHistoryEntry>>,
        completed: bool,
    ) {
        self.ImportSafariHistoryEntries(entries, completed);
    }
}

impl HistoryCallback<ffi::StablePortabilityHistoryEntry>
    for ffi::StablePortabilityHistoryCallbackFromRust
{
    fn import_entries(
        self: Pin<&mut Self>,
        entries: cxx::UniquePtr<CxxVector<ffi::StablePortabilityHistoryEntry>>,
        completed: bool,
    ) {
        self.ImportStablePortabilityHistoryEntries(entries, completed);
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
fn batch_and_send<T, U, C>(
    item: T,
    history: &mut cxx::UniquePtr<CxxVector<U>>,
    threshold: usize,
    callback: Pin<&mut C>,
) where
    T: Into<U>,
    U: 'static + cxx::vector::VectorElement + cxx::ExternType<Kind = cxx::kind::Trivial>,
    C: HistoryCallback<U> + ?Sized,
{
    history.as_mut().unwrap().push(item.into());
    if history.len() >= threshold {
        let batch_to_send = std::mem::replace(history, CxxVector::<U>::new());
        callback.import_entries(batch_to_send, /* completed= */ false);
    }
}

// Returns the expected extension for the provided file type.
fn expected_extension(file_type: ffi::FileType) -> Result<&'static str> {
    match file_type {
        ffi::FileType::Bookmarks => Ok("html"),
        ffi::FileType::SafariHistory => Ok("json"),
        ffi::FileType::Passwords => Ok("csv"),
        ffi::FileType::PaymentCards => Ok("json"),
        _ => Err(anyhow!("Unknown file type")),
    }
}

// Verifies if the file in the provided path has the desired extension.
fn has_extension(path: &Path, file_type: ffi::FileType) -> bool {
    let Ok(ext) = expected_extension(file_type) else {
        return false;
    };

    path.extension().map_or(false, |actual_extension| actual_extension.eq_ignore_ascii_case(ext))
}

// Returns the expected data type for the provided file type.
fn expected_data_type(file_type: ffi::FileType) -> Result<&'static str> {
    match file_type {
        ffi::FileType::SafariHistory => Ok("history"),
        ffi::FileType::StablePortabilityHistory => Ok("history_visits"),
        ffi::FileType::PaymentCards => Ok("payment_cards"),
        _ => Err(anyhow!("No data type for this file type")),
    }
}

// Returns the expected array token for the provided file type.
fn array_token_for_data_type(file_type: ffi::FileType) -> Result<&'static str> {
    match file_type {
        ffi::FileType::SafariHistory => Ok("history"),
        ffi::FileType::StablePortabilityHistory => Ok("history_visits"),
        ffi::FileType::PaymentCards => Ok("payment_cards"),
        _ => Err(anyhow!("No array token for this file type")),
    }
}

/// A custom reader that wraps a `zip::read::ZipFile` to implement
/// `io::BufRead`. This allows `serde_json_lenient` to efficiently read from the
/// zip entry without loading the entire entry into memory.
struct ZipEntryBufReader<'a, R: Read> {
    inner: BufReader<zip::read::ZipFile<'a, R>>,
}

impl<'a, R: Read> ZipEntryBufReader<'a, R> {
    fn new(zip_file: zip::read::ZipFile<'a, R>) -> Self {
        ZipEntryBufReader { inner: BufReader::with_capacity(STREAM_BUFFER_SIZE, zip_file) }
    }
}

struct ArrayDeserializerSeed<'de, T>(Box<dyn FnMut(T) + 'de>)
where
    T: Deserialize<'de>;

impl<'de, 'a, T> de::DeserializeSeed<'de> for ArrayDeserializerSeed<'de, T>
where
    T: Deserialize<'de>,
{
    // The return type of the `deserialize` method. This implementation
    // passes elements into `callback` but does not create any new data
    // structure, so the return type is ().
    type Value = ();

    fn deserialize<D>(self, deserializer: D) -> Result<(), D::Error>
    where
        D: de::Deserializer<'de>,
    {
        struct SeqVisitor<'de, T>(Box<dyn FnMut(T) + 'de>);

        impl<'de, T> de::Visitor<'de> for SeqVisitor<'de, T>
        where
            T: Deserialize<'de>,
        {
            type Value = ();

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("array")
            }

            fn visit_seq<S>(mut self, mut seq: S) -> Result<(), S::Error>
            where
                S: de::SeqAccess<'de>,
            {
                while let Some(t) = seq.next_element::<T>()? {
                    self.0(t);
                }
                Ok(())
            }
        }

        deserializer.deserialize_seq(SeqVisitor(self.0))
    }
}

fn deserialize_top_level<'de, T, R>(
    mut stream_reader: BufReader<R>,
    file_type: ffi::FileType,
    callback: impl FnMut(T) + 'de,
    metadata_only: bool,
) -> Result<()>
where
    T: Deserialize<'de> + 'de,
    R: std::io::Read,
{
    const VALID_PARTIAL_DESERIALIZATION: &'static str = "Valid partial deserialization";

    struct MapVisitor<'de, T>
    where
        T: Deserialize<'de>,
    {
        file_type: ffi::FileType,
        callback: Box<dyn FnMut(T) + 'de>,
        metadata_only: bool,
    }

    impl<'de, T> de::Visitor<'de> for MapVisitor<'de, T>
    where
        T: Deserialize<'de> + 'de,
    {
        type Value = ();

        fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
            formatter.write_str("map/object")
        }

        fn visit_map<M>(self, mut map: M) -> Result<(), M::Error>
        where
            M: de::MapAccess<'de>,
        {
            const METADATA_TOKEN: &'static str = "metadata";
            let Ok(data_type) = expected_data_type(self.file_type) else {
                return Err(DeserializerError::custom("File type has no associated data type"));
            };
            let Ok(expected_key) = array_token_for_data_type(self.file_type) else {
                return Err(DeserializerError::custom("File type has no associated array token"));
            };
            let mut has_expected_data_type = false;

            while let Some(actual_key) = map.next_key::<String>()? {
                if actual_key == METADATA_TOKEN {
                    if has_expected_data_type {
                        return Err(DeserializerError::custom("Multiple metadata tokens"));
                    }
                    let metadata = map.next_value::<Metadata>()?;
                    has_expected_data_type = metadata.data_type == data_type;
                    if !has_expected_data_type {
                        return Err(DeserializerError::custom("Unexpected data type"));
                    } else if self.metadata_only {
                        // If only the data type check is required, it has been performed
                        // successfully, so no further deserialization is required. To prevent
                        // deserialize_map from generating an error caused by the deserialization
                        // being incomplete, a valid partial deserialization error is returned here
                        // and will be interpreted as a valid result below.
                        return Err(DeserializerError::custom(VALID_PARTIAL_DESERIALIZATION));
                    }
                } else if actual_key == expected_key {
                    if !has_expected_data_type {
                        return Err(DeserializerError::custom("Found array before metadata"));
                    }
                    map.next_value_seed(ArrayDeserializerSeed(Box::new(self.callback)))?;
                    // At this point, the user data array has been parsed successfully, so no
                    // further deserialization is required. To prevent deserialize_map from
                    // generating an error caused by the deserialization being incomplete, a valid
                    // partial deserialization error is returned here and will be interpreted as a
                    // valid result below.
                    return Err(DeserializerError::custom(VALID_PARTIAL_DESERIALIZATION));
                } else {
                    let de::IgnoredAny = map.next_value()?;
                }
            }

            Err(DeserializerError::custom("Array not found"))
        }
    }

    let callback = Box::new(callback);
    let mut d = serde_json_lenient::Deserializer::from_reader(&mut stream_reader);
    match d.deserialize_map(MapVisitor { file_type, callback, metadata_only }) {
        Ok(_) => Ok(()),
        Err(e) => {
            // If the error is a valid partial deserialization error, then all the required
            // tasks have been completed successfully and deserialization was stopped early
            // to prevent any further unnecessary work, so Ok(()) can be returned in this
            // case.
            if e.to_string().starts_with(VALID_PARTIAL_DESERIALIZATION) {
                return Ok(());
            }
            return Err(anyhow!("JSON parsing error: {}", e));
        }
    }
}

// Attempts to parse the history file. Returns whether parsing was successful.
fn parse_history_file<'a, R: Read>(
    stream_reader: ZipEntryBufReader<'a, R>,
    callback: impl FnMut(SafariHistoryJSONEntry) + 'a,
) -> bool {
    return deserialize_top_level(
        stream_reader.inner,
        ffi::FileType::SafariHistory,
        callback,
        /* metadata_only= */ false,
    )
    .is_ok();
}

// Attempts to parse a Chrome history file. Returns whether parsing was
// successful.
fn parse_stable_portability_history(
    json_filename: &[u8],
    mut history_callback: cxx::UniquePtr<ffi::StablePortabilityHistoryCallbackFromRust>,
    history_size_threshold: usize,
) -> bool {
    let mut history = CxxVector::<ffi::StablePortabilityHistoryEntry>::new();
    let result = (|| -> Result<()> {
        let path_str = std::str::from_utf8(json_filename)?;
        let file = fs::File::open(path_str)?;
        let stream_reader = BufReader::with_capacity(STREAM_BUFFER_SIZE, file);
        deserialize_top_level::<StablePortabilityHistoryJSONEntry, std::fs::File>(
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
    // Send final batch if any, and completion signal.
    history_callback.as_mut().unwrap().import_entries(history, true);
    return result.is_ok();
}

// Returns whether the file used by the stream reader is a history file.
fn is_history_file<'a, R: Read>(stream_reader: ZipEntryBufReader<'a, R>) -> bool {
    return deserialize_top_level::<SafariHistoryJSONEntry, zip::read::ZipFile<'a, R>>(
        stream_reader.inner,
        ffi::FileType::SafariHistory,
        |_| {},
        /* metadata_only= */ true,
    )
    .is_ok();
}

// Attempts to parse the payment cards file. Returns whether parsing was
// successful.
fn parse_payment_cards_file<'a, R: Read>(
    stream_reader: ZipEntryBufReader<'a, R>,
    callback: impl FnMut(PaymentCardJSONEntry) + 'a,
) -> bool {
    return deserialize_top_level(
        stream_reader.inner,
        ffi::FileType::PaymentCards,
        callback,
        /* metadata_only= */ false,
    )
    .is_ok();
}

// Returns whether the file used by the stream reader is a payment cards file.
fn is_payment_cards_file<'a, R: Read>(stream_reader: ZipEntryBufReader<'a, R>) -> bool {
    return deserialize_top_level::<PaymentCardJSONEntry, zip::read::ZipFile<'a, R>>(
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
    fn fold_files<F, B>(&mut self, init: B, mut f: F) -> B
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

    fn get_file_size_bytes(&mut self, file_type: ffi::FileType) -> u64 {
        // Since there can be multiple history files, we need to sum the sizes of all of
        // them.
        if file_type == ffi::FileType::SafariHistory {
            return self.fold_files(0u64, |mut total_file_size_bytes, file, outpath| {
                if has_extension(outpath, file_type) {
                    let file_size_bytes = file.size();
                    let stream_reader = ZipEntryBufReader::new(file);
                    if is_history_file(stream_reader) {
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

    fn unzip(
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

    fn parse_safari_history(
        self: &mut ZipFileArchive,
        mut history_callback: cxx::UniquePtr<ffi::SafariHistoryCallbackFromRust>,
        history_size_threshold: usize,
    ) {
        let mut history = CxxVector::<ffi::SafariHistoryEntry>::new();
        self.fold_files((), |(), file, outpath| {
            if has_extension(outpath, ffi::FileType::SafariHistory) {
                let stream_reader = ZipEntryBufReader::new(file);
                parse_history_file(stream_reader, |history_item| {
                    batch_and_send(
                        history_item,
                        &mut history,
                        history_size_threshold,
                        history_callback.as_mut().unwrap(),
                    );
                });
            }
        });

        history_callback.as_mut().unwrap().import_entries(history, /* completed= */ true);
    }

    fn parse_payment_cards(
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
