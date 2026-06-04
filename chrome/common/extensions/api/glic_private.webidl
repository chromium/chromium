// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum ProfileReadyState {
  // Unknown failure, not ready.
  "error",
  // Would be ready if the user updated their profile sign in state.
  "sign-in-required",
  // Ready to use Gemini
  "ready",
  // Not eligible to use Gemini in Chrome due to admin controls.
  "disabled-by-admin",
  // Not eligible to use Gemini in Chrome based on account capability
  // values.
  "ineligible",
  // Not eligible due to country location mismatch.
  "location-mismatch",
  // Not eligible due to account capabilities.
  "ineligible-account"
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
  required boolean userEnableActuationOnWeb;
};

enum InvocationSource {
  "unknown",
  "universal-cart",
  "promotion-page"
};

dictionary InvokeDetails {
  // The prompt ID to lookup from Chrome, required unless called from the promotion page.
  DOMString promptId;

  // The source of the invocation.
  required InvocationSource invocationSource;

  // Document ID of the page that originated the invocation.
  // This is provided by the caller (the extension background page) to specify
  // the context of the user's action, since the API itself is called from the
  // background context.
  required DOMString documentId;

  // Whether should invoke the task in a new tab. Default to false.
  boolean inNewTab;
};

enum ErrorCode {
  "local-invalid-invocation-source",
  "local-missing-prompt-id",
  "server-missing-prompt",
  "http-error",
  "parse-error",
  "local-no-active-tab",
  "local-glic-not-enabled",
  "local-glic-not-ready",
  "local-glic-actuation-not-allowed",
  "local-glic-not-enabled-and-consented",
  "local-account-mismatch",
  "local-invalid-document-id",
  "local-conversation-not-found",
  "local-no-bound-tabs",
  "local-tab-not-in-window"
};

// Private API for Gemini (Glic) synchronization.
[implemented_in="chrome/browser/extensions/api/glic_private/glic_private_api.h"]
interface GlicPrivate {
  // Retrieves the current Glic state for the profile.
  // |Returns|: Promise that resolves to the current Glic state.
  // |PromiseValue|: state: The current Glic state.
  static Promise<ProfileState> getState(DOMString documentId);

  // Invokes glic with details.
  // |Returns|: Promise that resolves when invocation is successful.
  static Promise<undefined> invoke(InvokeDetails details);

  // Checks whether a particular conversation ID is present.
  // |Returns|: Promise that resolves to true if the conversation is present.
  // |PromiseValue|: isPresent: True if conversation is present, false otherwise.
  static Promise<boolean> hasConversation(DOMString conversationId);

  // Activates a tab with a specific conversation open in the side panel.
  // |Returns|: Promise that resolves when the activation operation is
  // successful.
  static Promise<undefined> activateTabWithConversation(
      DOMString conversationId);
};

partial interface Browser {
  static attribute GlicPrivate glicPrivate;
};
