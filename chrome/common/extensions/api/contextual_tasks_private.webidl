// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary ProfileState {
  // Whether the profile is eligible for Contextual Tasks.
  required boolean isEligible;
};

// LINT.IfChange(AimParams)
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

  // The search query.
  DOMString q;
};
// LINT.ThenChange(//chrome/browser/resources/contextual_tasks/extension/background.ts:AimParams,//chrome/browser/resources/contextual_tasks/extension/background.ts:AimParamsCall,//chrome/browser/extensions/api/contextual_tasks_private/contextual_tasks_private_api.cc:AimParamsCpp)


dictionary LaunchPanelInNewTabDetails {
  // The query parameters to construct the AIM URL.
  required AimParams aimParams;

  // The URL to load in the new tab.
  required DOMString targetUrl;
  required DOMString documentId;
};

// LINT.IfChange(AimPromoParams)

dictionary AimPromoParams {
  // Magi State Token. Restores the AI Overview as the first turn of an AIM
  // thread.
  required DOMString mstk;

  // How to handle the AIM handover. See `AimParams.aioh`.
  DOMString aioh;

  // Where to restore the AIM thread from. See `AimParams.csuir`.
  DOMString csuir;

  // XSRF token for the restore navigation. See go/gws-xsleaks-proposal.
  DOMString sxsrf;
};
// LINT.ThenChange(//chrome/browser/resources/contextual_tasks/extension/background.ts:AimPromoParamsCall,//chrome/browser/extensions/api/contextual_tasks_private/contextual_tasks_private_api.cc:AimPromoParamsCpp)

// Coarse labels that Search can send about the query. Chrome only uses them to
// slice metrics.
enum AimPromoQueryHint {
  "edu_homework",
  "edu_conceptual",
  "how_to_web_app"
};

dictionary RegisterAimThreadForPromoDetails {
  // Used to build the AIM restore URL if the user accepts the cue.
  required AimPromoParams aimPromoParams;

  // Never shown in UI or added to the restore URL.
  sequence<AimPromoQueryHint> queryHints;

  // The documentId of the Search Results Page that sent the parameters.
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

  // Registers AIM thread restore parameters for a Search Results Page, so
  // Chrome may later show a contextual cue that reopens its AI Overview in the
  // Contextual Tasks side panel. Never shows UI itself.
  // |Returns|: Promise that resolves once the parameters are handled, whether
  // or not Chrome keeps them.
  static Promise<undefined> registerAimThreadForPromo(
      RegisterAimThreadForPromoDetails details);
};

partial interface Browser {
  static attribute ContextualTasksPrivate contextualTasksPrivate;
};
