// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary ProfileState {
  // Whether the profile is eligible for Contextual Tasks.
  required boolean isEligible;
};

dictionary LaunchPanelInNewTabDetails {
  // The URL to load in the tab.
  required DOMString targetUrl;

  // The AIM URL to load in the Contextual Tasks side panel.
  required DOMString aimUrl;

  // The RenderFrameHost Document ID of the caller webpage.
  required DOMString documentId;
};

interface ContextualTasksPrivate {
  // Returns the profile state for Contextual Tasks.
  // |PromiseValue|: state: The current ProfileState for Contextual Tasks.
  static Promise<ProfileState> getState(DOMString documentId);

  // Launches the Contextual Tasks panel in a new tab with details.
  // |Returns|: Promise that resolves when the launch is successful.
  static Promise<undefined> launchPanelInNewTab(
      LaunchPanelInNewTabDetails details);
};

partial interface Browser {
  static attribute ContextualTasksPrivate contextualTasksPrivate;
};
