// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum ProfileReadyState {
  // Unknown failure, not ready.
  "ERROR",
  // Would be ready if the user updated their profile sign in state.
  "SIGN_IN_REQUIRED",
  // Ready to use Gemini
  "READY",
  // Not eligible to use Gemini in Chrome due to admin controls.
  "DISABLED_BY_ADMIN",
  // Not eligible to use Gemini in Chrome based on account capability
  // values.
  "INELIGIBLE"
};

dictionary ProfileState {
  // Computed high-level states

  required boolean isEnabled;
  required boolean isEnabledAndConsented;
  required ProfileReadyState readyState;

  // Per-feature capabilities

  required boolean liveAllowed;
  required boolean shareImageAllowed;
  required boolean actuationAllowed;
};

enum InvocationSource {
  "Unknown",
  "UniversalCart"
};

dictionary InvokeDetails {
  // The prompt ID to lookup from Chrome.
  required DOMString promptId;

  // The source of the invocation.
  required InvocationSource invocationSource;

  // Whether should invoke the task in a new tab. Default to false.
  boolean inNewTab;
};

// Private API for Gemini (Glic) synchronization.
[implemented_in="chrome/browser/extensions/api/glic_private/glic_private_api.h"]
interface GlicPrivate {
  // Retrieves the current Glic state for the profile.
  // |Returns|: Promise that resolves to the current Glic state.
  // |PromiseValue|: state: The current Glic state.
  static Promise<ProfileState> getState();

  // Invokes glic with details.
  static Promise<undefined> invoke(InvokeDetails details);
};

partial interface Browser {
  static attribute GlicPrivate glicPrivate;
};
