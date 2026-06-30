// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary ProfileState {
  // Whether the profile is eligible for Contextual Tasks.
  required boolean isEligible;
};

dictionary AimParams {
  // Whether the link click came from a Nitrogen tab. Used for metrics.
  DOMString ntc;

  // Magi State Token which contains an opaque encrypted and encoded event ID to
  // keep track of in scope state. Used to restore the AIO response as an AIM
  // thread.
  DOMString mstk;

  // Param to indicate how to handle the AIM handover.
  //  * "1" indicates that this is an AIO handover request to AIM.
  //  * "2" indicates that this is a SRP to AIM desktop direct handover
  //    request.
  //  * "3" indicates that this request is part of a session for which we have
  //    previously performed a handover from SRP to AIM.
  DOMString aioh;

  // Param to indicate where to restore the AIM thread from:
  //  * 1 - restore a previous conversation turn
  //  * 2 - restore as a follow up from a SRP request
  //  * 3 - restore as a Magi response from Memory (server)
  DOMString csuir;

  // Visual element data in Base64 proto or text split by ':' and ','. Used for
  // click tracking.
  DOMString ved;

  // Query param indicating dark mode (=1) or light mode (=0).
  DOMString cs;

  // XSRF token for search requests. See go/gws-xsleaks-proposal.
  DOMString sxsrf;

  // Event ID for the query. Used for logging.
  DOMString ei;
};

dictionary LaunchPanelInNewTabDetails {
  // The query parameters to construct the AIM URL.
  required AimParams aimParams;

  // The URL to load in the new tab.
  required DOMString targetUrl;
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
