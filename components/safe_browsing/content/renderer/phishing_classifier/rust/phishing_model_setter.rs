// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Renderer-side implementation of `safe_browsing::mojom::PhishingModelSetter`.
//!
//! The browser process pushes updated client-side phishing TfLite models to
//! each renderer over this interface. Installing a model constructs a C++
//! `Scorer` (via `scorer_bridge`) and stores it in the process-wide
//! `ScorerStorage` singleton used by the renderer's phishing classifier.

chromium::import! {
    "//base:file";
    "//components/safe_browsing/content/common:interfaces_rust" as safe_browsing_mojom;
    "//components/safe_browsing/content/renderer/phishing_classifier/rust:scorer_bridge" as scorer;
    "//mojo/public/rust/bindings";
}
// Try very hard to avoid using locks in chromium. You should prefer to use
// wrappers that provide access based on sequence checking rather than locking.
// Unfortunately, those don't exist yet (but they're coming soon!), and these
// are only used in tests, so we'll use a Mutex for now.
// TODO(crbug.com/470438844): Replace Arc/Mutex with sequenced equivalents once
// available.
use std::sync::{Arc, Mutex};

use bindings::remote::{PendingRemote, Remote};
use file::File;
use safe_browsing_mojom::safe_browsing::{PhishingModelSetter, PhishingModelSetterTestObserver};

/// Per-connection `PhishingModelSetter` state object owned by a Mojo
/// `Receiver`.
///
/// The browser opens a new `PhishingModelSetter` connection for each model
/// push. In Rust Mojo, each `Receiver` owns its state object for the lifetime
/// of the connection, and state objects cannot be unbound from one connection
/// and rebound to another. The test observer therefore lives behind an `Arc`
/// so that cloning `PhishingModelSetterImpl` shares it across successive
/// receiver bindings on the render thread.
#[derive(Clone, Default)]
pub struct PhishingModelSetterImpl {
    /// Optional observer notified via `PhishingModelUpdated()` whenever a new
    /// primary classification model is successfully installed. Used by browser
    /// and unit tests.
    observer_for_testing: Arc<Mutex<Option<Remote<dyn PhishingModelSetterTestObserver>>>>,
}

impl PhishingModelSetterImpl {
    pub fn new() -> Self {
        Self::default()
    }

    fn notify_observer(&self) {
        if let Some(observer) = self.observer_for_testing.lock().unwrap().as_mut() {
            observer.PhishingModelUpdated();
        }
    }
}

// `PhishingModelSetter` here refers to the mojom `interface`; this block
// tells Mojo how to respond when a receiver holding the `Impl` gets
// each possible message.
impl PhishingModelSetter for PhishingModelSetterImpl {
    fn SetImageEmbeddingAndPhishingTfLiteModel(
        &mut self,
        classification_input_width: i32,
        classification_input_height: i32,
        classification_model: Option<Box<File>>,
        image_embedding_input_width: i32,
        image_embedding_input_height: i32,
        image_embedding_model: Option<Box<File>>,
    ) {
        if scorer::set_image_embedding_and_phishing_tflite_model(
            classification_input_width,
            classification_input_height,
            classification_model.map(|f| *f),
            image_embedding_input_width,
            image_embedding_input_height,
            image_embedding_model.map(|f| *f),
        ) {
            self.notify_observer();
        }
    }

    fn AttachImageEmbeddingModelAndDimensions(
        &mut self,
        image_embedding_input_width: i32,
        image_embedding_input_height: i32,
        image_embedding_model: Option<Box<File>>,
    ) {
        // This updates the existing Scorer instead of adding a new one,
        // so it doesn't trigger `PhishingModelUpdated` and is ignored if
        // no Scorer is currently installed.
        scorer::attach_image_embedding_model_and_dimensions(
            image_embedding_input_width,
            image_embedding_input_height,
            image_embedding_model.map(|f| *f),
        );
    }

    fn SetPhishingTfLiteModel(
        &mut self,
        classification_input_width: i32,
        classification_input_height: i32,
        classification_model: Option<Box<File>>,
    ) {
        if scorer::set_phishing_tflite_model(
            classification_input_width,
            classification_input_height,
            classification_model.map(|f| *f),
        ) {
            self.notify_observer();
        }
    }

    fn ClearScorer(&mut self) {
        scorer::clear_scorer();
    }

    fn SetTestObserver(
        &mut self,
        observer: Option<PendingRemote<dyn PhishingModelSetterTestObserver>>,
        response_callback: impl FnOnce(),
    ) {
        *self.observer_for_testing.lock().unwrap() = observer.map(|remote| remote.bind());
        response_callback();
    }
}

// Since we just implemented a mojom `interface`, we need to call this macro to
// set some things up in the background.
bindings::register_mojom_state_object_impls!(
    impl PhishingModelSetter for PhishingModelSetterImpl);
