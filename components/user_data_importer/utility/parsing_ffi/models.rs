// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ffi;
use serde::Deserialize;

// Safari's browser history JSON format, as documented here:
// https://developer.apple.com/documentation/safariservices/importing-data-exported-from-safari?language=objc#Import-browser-history
#[derive(Deserialize)]
pub struct SafariHistoryJSONEntry {
    // A string that’s the URL of the history item.
    pub url: String,

    // An optional string that, if present, is the title of the history item.
    pub title: Option<String>,

    // An integer that’s the UNIX timestamp in microseconds of the latest visit to the item.
    pub time_usec: u64,

    // An optional string that, if present, is the URL of the next item in the redirect chain.
    pub destination_url: Option<String>,

    // An optional integer that’s present if destination_url is also present and is the UNIX
    // timestamp (the number of microseconds since midnight UTC, January 1, 1970) of the next
    // navigation in the redirect chain.
    // UNUSED: destination_time_usec: Option<u64>,

    // An optional string that, if present, is the URL of the previous item in the redirect
    // chain.
    pub source_url: Option<String>,

    // An optional integer that’s present if source_url is also present and is the UNIX
    // timestamp in microseconds of the previous navigation in the redirect chain.
    // UNUSED: source_time_usec: Option<u64>,

    // An integer that’s the number of visits the browser made to this item, and is always
    // greater than or equal to 1.
    pub visit_count: u64,
    //
    // An optional Boolean that’s true if Safari failed to load the site when someone most
    // recently tried to access it; otherwise, it’s false.
    // UNUSED: latest_visit_was_load_failure: Option<bool>,

    // An optional Boolean that’s true if the last visit to this item used the HTTP GET method;
    // otherwise, it’s false.
    // UNUSED: latest_visit_was_http_get: Option<bool>,
}

// Stable Portability data format.
// JSON file with one entry per history visit.
#[derive(Deserialize)]
pub struct StablePortabilityHistoryJSONEntry {
    // Optional: boolean that represents whether this browsing history visit has been saved to or
    // syncing to a server-side account by the browser. Defaults to false.
    pub synced: Option<bool>,

    // Required: title of the page that was visited.
    pub title: String,

    // Required: URL of the page that was visited.
    pub url: String,

    // Required: timestamp in which the navigation took place in microseconds since the
    // Unix epoch.
    pub visit_time_unix_epoch_usec: u64,

    // Optional: integer indicating the number of visits this entry represents. Defaults
    // to 1.
    pub visit_count: Option<u64>,

    // Optional: integer indicating how many of the visits were typed into the omnibox.
    // Defaults to 0.
    pub typed_count: Option<u64>,
}

// Safari's payment cards JSON format, as documented here:
// https://developer.apple.com/documentation/safariservices/importing-data-exported-from-safari?language=objc#Import-payment-cards
#[derive(Deserialize)]
pub struct PaymentCardJSONEntry {
    // A string that is the payment card number.
    pub card_number: String,

    // An optional string that, if present, is the name the person gave to the payment card.
    pub card_name: Option<String>,

    // An optional string that, if present, is the name of the cardholder.
    pub cardholder_name: Option<String>,

    // An optional integer that, if present, is the month of the card’s expiration date.
    pub card_expiration_month: Option<u64>,

    // An optional integer that, if present, is the year of the card’s expiration date.
    pub card_expiration_year: Option<u64>,
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
pub struct Metadata {
    // A string that’s web browser name, which is Safari if someone exported the data from
    // Safari on iOS, iPadOS, macOS, or visionOS; or Safari Technology Preview if someone
    // exported the data from Safari Technology Preview on macOS.
    // UNUSED: browser_name: String,

    // A string that’s the version of Safari that exported the data, for example 18.2.
    // UNUSED: browser_version: String,

    // A string that describes the data in the file; one of history, extensions, or
    // payment_cards.
    pub data_type: String,
    //
    // An integer that’s the UNIX timestamp (the number of microseconds since midnight in the
    // UTC time zone on January 1, 1970) at which Safari exported the file.
    // UNUSED: export_time_usec: u64,

    // An integer that’s the version of the export schema.
    // UNUSED: schema_version: u64,
}

impl From<SafariHistoryJSONEntry> for ffi::SafariHistoryEntry {
    fn from(entry: SafariHistoryJSONEntry) -> Self {
        Self {
            url: entry.url,
            title: entry.title.unwrap_or(String::new()),
            time_usec: entry.time_usec,
            destination_url: entry.destination_url.unwrap_or(String::new()),
            source_url: entry.source_url.unwrap_or(String::new()),
            visit_count: entry.visit_count,
        }
    }
}

impl From<StablePortabilityHistoryJSONEntry> for ffi::StablePortabilityHistoryEntry {
    fn from(entry: StablePortabilityHistoryJSONEntry) -> Self {
        Self {
            synced: entry.synced.unwrap_or(false),
            url: entry.url,
            title: entry.title,
            visit_time_unix_epoch_usec: entry.visit_time_unix_epoch_usec,
            visit_count: entry.visit_count.unwrap_or(1),
            typed_count: entry.typed_count.unwrap_or(0),
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
