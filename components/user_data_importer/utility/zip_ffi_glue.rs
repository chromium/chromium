// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
        History,
        Passwords,
        PaymentCards,
    }

    // C++ interop version of the HistoryJSONEntry structure.
    // See HistoryJSONEntry for field documentation.
    struct HistoryEntry {
        url: String,
        title: String,
        time_usec: u64,
        visit_count: u64,
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

    extern "Rust" {
        type ResultOfZipFileArchive;
        fn err(self: &ResultOfZipFileArchive) -> bool;
        fn unwrap(self: &mut ResultOfZipFileArchive) -> Box<ZipFileArchive>;

        type ZipFileArchive;
        fn get_file_size(self: &mut ZipFileArchive, file_type: FileType) -> u64;
        fn unzip(
            self: &mut ZipFileArchive,
            file_type: FileType,
            mut output_bytes: Pin<&mut CxxString>,
        ) -> bool;
        fn parse_history(
            self: &mut ZipFileArchive,
            history: Pin<&mut CxxVector<HistoryEntry>>,
        ) -> bool;
        fn parse_payment_cards(
            self: &mut ZipFileArchive,
            history: Pin<&mut CxxVector<PaymentCardEntry>>,
        ) -> bool;

        fn new_archive(zip_filename: &[u8]) -> Box<ResultOfZipFileArchive>;
    }
}

// Safari's browser history JSON format, as documented here:
// https://developer.apple.com/documentation/safariservices/importing-data-exported-from-safari?language=objc#Import-browser-history
#[derive(Deserialize)]
struct HistoryJSONEntry {
    // A string that’s the URL of the history item.
    url: String,

    // An optional string that, if present, is the title of the history item.
    title: Option<String>,

    // An integer that’s the UNIX timestamp in microseconds of the latest visit to the item.
    time_usec: u64,

    // An optional string that, if present, is the URL of the next item in the redirect chain.
    // UNUSED: destination_url: Option<String>,

    // An optional integer that’s present if destination_url is also present and is the UNIX
    // timestamp (the number of microseconds since midnight UTC, January 1, 1970) of the next
    // navigation in the redirect chain.
    // UNUSED: destination_time_usec: Option<u64>,

    // An optional string that, if present, is the URL of the previous item in the redirect
    // chain.
    // UNUSED: source_url: Option<String>,

    // An optional integer that’s present if source_url is also present and is the UNIX
    // timestamp in microseconds of the previous navigation in the redirect chain.
    // UNUSED: source_time_usec: Option<u64>,

    // An integer that’s the number of visits the browser made to this item, and is always
    // greater than or equal to 1.
    visit_count: u64,
    //
    // An optional Boolean that’s true if Safari failed to load the site when someone most
    // recently tried to access it; otherwise, it’s false.
    // UNUSED: latest_visit_was_load_failure: Option<bool>,

    // An optional Boolean that’s true if the last visit to this item used the HTTP GET method;
    // otherwise, it’s false.
    // UNUSED: latest_visit_was_http_get: Option<bool>,
}

// Safari's payment cards JSON format, as documented here:
// https://developer.apple.com/documentation/safariservices/importing-data-exported-from-safari?language=objc#Import-payment-cards
#[derive(Deserialize)]
struct PaymentCardJSONEntry {
    // A string that is the payment card number.
    card_number: String,

    // An optional string that, if present, is the name the person gave to the payment card.
    card_name: Option<String>,

    // An optional string that, if present, is the name of the cardholder.
    cardholder_name: Option<String>,

    // An optional integer that, if present, is the month of the card’s expiration date.
    card_expiration_month: Option<u64>,

    // An optional integer that, if present, is the year of the card’s expiration date.
    card_expiration_year: Option<u64>,
    //
    // An optional integer that, if present, is the UNIX timestamp of the most recent occasion the
    // person used the payment card.
    // Note: Safari sometimes puts decimals here, which makes the parsing fail. Read as f64
    // instead.
    // UNUSED: card_last_used_time_usec: Option<f64>,
}

// Safari's metadata JSON format, as documented here:
// https://developer.apple.com/documentation/safariservices/importing-data-exported-from-safari?language=objc#Understand-JSON-metadata
#[derive(Deserialize)]
struct Metadata {
    // A string that’s web browser name, which is Safari if someone exported the data from
    // Safari on iOS, iPadOS, macOS, or visionOS; or Safari Technology Preview if someone
    // exported the data from Safari Technology Preview on macOS.
    // UNUSED: browser_name: String,

    // A string that’s the version of Safari that exported the data, for example 18.2.
    // UNUSED: browser_version: String,

    // A string that describes the data in the file; one of history, extensions, or
    // payment_cards.
    data_type: String,
    //
    // An integer that’s the UNIX timestamp (the number of microseconds since midnight in the
    // UTC time zone on January 1, 1970) at which Safari exported the file.
    // UNUSED: export_time_usec: u64,

    // An integer that’s the version of the export schema.
    // UNUSED: schema_version: u64,
}

impl From<HistoryJSONEntry> for ffi::HistoryEntry {
    fn from(entry: HistoryJSONEntry) -> Self {
        Self {
            url: entry.url,
            title: entry.title.unwrap_or(String::new()),
            time_usec: entry.time_usec,
            visit_count: entry.visit_count,
        }
    }
}

impl From<PaymentCardJSONEntry> for ffi::PaymentCardEntry {
    fn from(entry: PaymentCardJSONEntry) -> Self {
        Self {
            card_number: entry.card_number,
            card_name: entry.card_name.unwrap_or(String::new()),
            cardholder_name: entry.cardholder_name.unwrap_or(String::new()),
            card_expiration_month: entry.card_expiration_month.unwrap_or(0),
            card_expiration_year: entry.card_expiration_year.unwrap_or(0),
        }
    }
}

// Returns the expected extension for the provided file type.
fn expected_extension(file_type: ffi::FileType) -> Result<&'static str> {
    match file_type {
        ffi::FileType::Bookmarks => Ok("html"),
        ffi::FileType::History => Ok("json"),
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
        ffi::FileType::History => Ok("history"),
        ffi::FileType::PaymentCards => Ok("payment_cards"),
        _ => Err(anyhow!("No data type for this file type")),
    }
}

// Returns the expected array token for the provided file type.
fn array_token_for_data_type(file_type: ffi::FileType) -> Result<&'static str> {
    match file_type {
        ffi::FileType::History => Ok("history"),
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
    mut stream_reader: BufReader<zip::read::ZipFile<'de, R>>,
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
    callback: impl FnMut(HistoryJSONEntry) + 'a,
) -> bool {
    return deserialize_top_level::<HistoryJSONEntry, R>(
        stream_reader.inner,
        ffi::FileType::History,
        callback,
        /* metadata_only= */ false,
    )
    .is_ok();
}

// Returns whether the file used by the stream reader is a history file.
fn is_history_file<'a, R: Read>(stream_reader: ZipEntryBufReader<'a, R>) -> bool {
    return deserialize_top_level::<HistoryJSONEntry, R>(
        stream_reader.inner,
        ffi::FileType::History,
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
    return deserialize_top_level::<PaymentCardJSONEntry, R>(
        stream_reader.inner,
        ffi::FileType::PaymentCards,
        callback,
        /* metadata_only= */ false,
    )
    .is_ok();
}

// Returns whether the file used by the stream reader is a payment cards file.
fn is_payment_cards_file<'a, R: Read>(stream_reader: ZipEntryBufReader<'a, R>) -> bool {
    return deserialize_top_level::<PaymentCardJSONEntry, R>(
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
    fn get_file_size(&mut self, file_type: ffi::FileType) -> u64 {
        for i in 0..self.archive.len() {
            let Ok(file) = self.archive.by_index(i) else {
                continue;
            };
            let Some(outpath) = file.enclosed_name() else {
                continue;
            };

            // Read the first file matching the requested type found within the zip file.
            if has_extension(&outpath.as_path(), file_type) {
                if file_type == ffi::FileType::Bookmarks || file_type == ffi::FileType::Passwords {
                    return file.size();
                } else {
                    // Verify the data type in the JSON file.
                    let file_size = file.size();
                    let stream_reader = ZipEntryBufReader::new(file);
                    if file_type == ffi::FileType::History {
                        if is_history_file(stream_reader) {
                            return file_size;
                        }
                    } else if file_type == ffi::FileType::PaymentCards {
                        if is_payment_cards_file(stream_reader) {
                            return file_size;
                        }
                    }
                }
            }
        }

        0
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

        for i in 0..self.archive.len() {
            let Ok(mut file) = self.archive.by_index(i) else {
                continue;
            };
            let Some(outpath) = file.enclosed_name() else {
                continue;
            };
            if !has_extension(&outpath.as_path(), file_type) {
                continue;
            }

            // Read the first file matching the requested type found within the zip file.
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

        false
    }

    fn parse_history(
        self: &mut ZipFileArchive,
        mut history: Pin<&mut CxxVector<ffi::HistoryEntry>>,
    ) -> bool {
        for i in 0..self.archive.len() {
            let Ok(file) = self.archive.by_index(i) else {
                continue;
            };
            let Some(outpath) = file.enclosed_name() else {
                continue;
            };

            if has_extension(&outpath.as_path(), ffi::FileType::History) {
                let stream_reader = ZipEntryBufReader::new(file);
                if parse_history_file(stream_reader, |history_item| {
                    history.as_mut().push(history_item.into());
                }) {
                    return true;
                }
            }
        }

        false
    }

    fn parse_payment_cards(
        self: &mut ZipFileArchive,
        mut payment_cards: Pin<&mut CxxVector<ffi::PaymentCardEntry>>,
    ) -> bool {
        for i in 0..self.archive.len() {
            let Ok(file) = self.archive.by_index(i) else {
                continue;
            };
            let Some(outpath) = file.enclosed_name() else {
                continue;
            };

            if has_extension(&outpath.as_path(), ffi::FileType::PaymentCards) {
                let stream_reader = ZipEntryBufReader::new(file);
                if parse_payment_cards_file(stream_reader, |payment_card_item| {
                    payment_cards.as_mut().push(payment_card_item.into());
                }) {
                    return true;
                }
            }
        }

        false
    }
}
