// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This file provides the Rust declarations of the C++ scorer shim functions

chromium::import! {
    "//base:file" as base_file;
}

use base_file::File as RustFile;

#[cxx::bridge(namespace = "safe_browsing::rust_bridge")]
mod ffi {
    unsafe extern "C++" {
        include!("base/files/file.h");
        include!("components/safe_browsing/content/renderer/phishing_classifier/scorer_rust_shim.h");

        #[namespace = "base"]
        type File = base_file::ffi::File;

        fn SetPhishingTfLiteModel(
            classification_input_width: i32,
            classification_input_height: i32,
            classification_model: UniquePtr<File>,
        ) -> bool;

        fn SetImageEmbeddingAndPhishingTfLiteModel(
            classification_input_width: i32,
            classification_input_height: i32,
            classification_model: UniquePtr<File>,
            image_embedding_input_width: i32,
            image_embedding_input_height: i32,
            image_embedding_model: UniquePtr<File>,
        ) -> bool;

        fn AttachImageEmbeddingModelAndDimensions(
            image_embedding_input_width: i32,
            image_embedding_input_height: i32,
            image_embedding_model: UniquePtr<File>,
        );

        fn ClearScorer();
    }
}

fn into_unique_ptr(file: Option<RustFile>) -> cxx::UniquePtr<ffi::File> {
    file.map(cxx::UniquePtr::from).unwrap_or_else(cxx::UniquePtr::null)
}

// The following functions are Rust-side shims, that just translate
// Option<RustFile> into UniquePtr<ffi::File>. Sadly `cxx` doesn't support
// options natively, or we could have built this into the bridge.

/// Builds a `Scorer` from `classification_model` and installs it in
/// `ScorerStorage`. Returns true if a `Scorer` was installed.
pub fn set_phishing_tflite_model(
    classification_input_width: i32,
    classification_input_height: i32,
    classification_model: Option<RustFile>,
) -> bool {
    ffi::SetPhishingTfLiteModel(
        classification_input_width,
        classification_input_height,
        into_unique_ptr(classification_model),
    )
}

/// Builds a `Scorer` with both a classification model and an image-embedding
/// model and installs it in `ScorerStorage`. Returns true if a `Scorer` was
/// installed.
pub fn set_image_embedding_and_phishing_tflite_model(
    classification_input_width: i32,
    classification_input_height: i32,
    classification_model: Option<RustFile>,
    image_embedding_input_width: i32,
    image_embedding_input_height: i32,
    image_embedding_model: Option<RustFile>,
) -> bool {
    ffi::SetImageEmbeddingAndPhishingTfLiteModel(
        classification_input_width,
        classification_input_height,
        into_unique_ptr(classification_model),
        image_embedding_input_width,
        image_embedding_input_height,
        into_unique_ptr(image_embedding_model),
    )
}

/// Attaches an image-embedding model to the `Scorer` currently in
/// `ScorerStorage`. Does nothing if no `Scorer` is installed.
pub fn attach_image_embedding_model_and_dimensions(
    image_embedding_input_width: i32,
    image_embedding_input_height: i32,
    image_embedding_model: Option<RustFile>,
) {
    ffi::AttachImageEmbeddingModelAndDimensions(
        image_embedding_input_width,
        image_embedding_input_height,
        into_unique_ptr(image_embedding_model),
    )
}

/// Clears the active `Scorer` in `ScorerStorage`.
pub fn clear_scorer() {
    ffi::ClearScorer()
}
