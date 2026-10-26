// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Lets C++ (`PhishingModelSetterImpl`) hand incoming `PhishingModelSetter`
//! receivers to the Rust implementation.

chromium::import! {
    "//components/safe_browsing/content/common:interfaces_rust" as safe_browsing_mojom;
    "//components/safe_browsing/content/renderer/phishing_classifier/rust:phishing_model_setter";
    "//mojo/public/rust/bindings";
}

use bindings::receiver::PendingAssociatedReceiver;
use bindings::CxxPendingAssociatedEndpoint;
use phishing_model_setter::PhishingModelSetterImpl;
use safe_browsing_mojom::safe_browsing::PhishingModelSetter;

#[cxx::bridge(namespace = "safe_browsing::rust_bridge")]
mod ffi {
    #[namespace = "mojo::rust::bindings"]
    unsafe extern "C++" {
        include!(
            "mojo/public/rust/bindings/multiplex_router/cpp_interop/associated_endpoint_rust_adapter.h"
        );

        #[cxx_name = "AssociatedEndpointRustAdapter"]
        // This tells `cxx` that the type already appears in another bridge,
        // so it just needs to import the existing bindings.
        type CxxPendingAssociatedEndpoint = super::CxxPendingAssociatedEndpoint;
    }

    extern "Rust" {
        type PhishingModelSetterState;

        fn CreatePhishingModelSetterState() -> Box<PhishingModelSetterState>;

        fn BindPhishingModelSetterReceiver(
            state: &PhishingModelSetterState,
            endpoint: UniquePtr<CxxPendingAssociatedEndpoint>,
        );
    }
}

/// State shared by every `PhishingModelSetter` connection bound with it.
///
/// The browser opens a new connection for each model push, and each
/// connection gets its own receiver holding a clone of the wrapped
/// `PhishingModelSetterImpl`. The clones share their state, which is what lets
/// a test observer registered on one connection see model updates pushed over
/// later ones.
///
/// The C++ `PhishingModelSetterImpl` owns this object, so (like the C++
/// implementation's own state) there is one per renderer process in
/// production, and a fresh one for each unit test.
//
// This is a newtype, rather than exposing `PhishingModelSetterImpl` directly,
// because `cxx` implements a trait for each `extern "Rust"` type, which the
// orphan rule only permits for types defined in this crate.
struct PhishingModelSetterState(PhishingModelSetterImpl);

#[allow(non_snake_case)]
fn CreatePhishingModelSetterState() -> Box<PhishingModelSetterState> {
    Box::new(PhishingModelSetterState(PhishingModelSetterImpl::new()))
}

/// Binds `endpoint` as a self-owned receiver sharing `state`. The receiver
/// lives until the browser drops its end of the connection.
#[allow(non_snake_case)]
fn BindPhishingModelSetterReceiver(
    state: &PhishingModelSetterState,
    endpoint: cxx::UniquePtr<CxxPendingAssociatedEndpoint>,
) {
    PendingAssociatedReceiver::<dyn PhishingModelSetter>::from_cpp(endpoint)
        .bind_self_owned(state.0.clone());
}
