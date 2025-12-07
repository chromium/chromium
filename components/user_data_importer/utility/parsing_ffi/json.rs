// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ffi;
use crate::models::Metadata;
use serde::{de, de::Deserializer, de::Error as DeserializerError};
use std::fmt;
use std::io::{BufReader, Read};
use zip;

pub const STREAM_BUFFER_SIZE: usize = 4096;

// Returns the expected data type for the provided file type.
fn expected_data_type(file_type: ffi::FileType) -> Result<&'static str, &'static str> {
    match file_type {
        ffi::FileType::SafariHistory => Ok("history"),
        ffi::FileType::StablePortabilityHistory => Ok("history_visits"),
        ffi::FileType::PaymentCards => Ok("payment_cards"),
        _ => Err("No data type for this file type"),
    }
}

// Returns the expected array token for the provided file type.
fn array_token_for_data_type(file_type: ffi::FileType) -> Result<&'static str, &'static str> {
    match file_type {
        ffi::FileType::SafariHistory => Ok("history"),
        ffi::FileType::StablePortabilityHistory => Ok("history_visits"),
        ffi::FileType::PaymentCards => Ok("payment_cards"),
        _ => Err("No array token for this file type"),
    }
}

/// A custom reader that wraps a `zip::read::ZipFile` to implement
/// `io::BufRead`. This allows `serde_json_lenient` to efficiently read from the
/// zip entry without loading the entire entry into memory.
pub struct ZipEntryBufReader<'a, R: Read> {
    pub inner: BufReader<zip::read::ZipFile<'a, R>>,
}

impl<'a, R: Read> ZipEntryBufReader<'a, R> {
    pub fn new(zip_file: zip::read::ZipFile<'a, R>) -> Self {
        ZipEntryBufReader { inner: BufReader::with_capacity(STREAM_BUFFER_SIZE, zip_file) }
    }
}

struct ArrayDeserializerSeed<'de, T>(Box<dyn FnMut(T) + 'de>)
where
    T: de::DeserializeOwned;

impl<'de, 'a, T> de::DeserializeSeed<'de> for ArrayDeserializerSeed<'de, T>
where
    T: de::DeserializeOwned,
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
            T: de::DeserializeOwned,
        {
            type Value = ();

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("array")
            }

            fn visit_seq<S>(mut self, mut seq: S) -> Result<(), S::Error>
            where
                S: de::SeqAccess<'de>,
            {
                while let Some(value) = seq.next_element::<serde_json_lenient::Value>()? {
                    if let Ok(t) = serde_json_lenient::from_value(value) {
                        self.0(t);
                    }
                }
                Ok(())
            }
        }

        deserializer.deserialize_seq(SeqVisitor(self.0))
    }
}

pub fn deserialize_top_level<'de, T, R>(
    mut stream_reader: BufReader<R>,
    file_type: ffi::FileType,
    callback: impl FnMut(T) + 'de,
    metadata_only: bool,
) -> Result<(), String>
where
    T: de::DeserializeOwned + 'de,
    R: std::io::Read,
{
    const VALID_PARTIAL_DESERIALIZATION: &'static str = "Valid partial deserialization";

    struct MapVisitor<'de, T>
    where
        T: de::DeserializeOwned,
    {
        file_type: ffi::FileType,
        callback: Box<dyn FnMut(T) + 'de>,
        metadata_only: bool,
    }

    impl<'de, T> de::Visitor<'de> for MapVisitor<'de, T>
    where
        T: de::DeserializeOwned + 'de,
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
            return Err(e.to_string());
        }
    }
}
