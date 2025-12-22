// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_INVOCATION_SOURCE_H_
#define COMPONENTS_LENS_LENS_OVERLAY_INVOCATION_SOURCE_H_

namespace lens {

// Designates the source of any lens overlay invocation (in other words, any
// call to `LensOverlayController::ShowUI()`).
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LensOverlayInvocationSource)
enum class LensOverlayInvocationSource {
  // The Chrome app ("3-dot") menu entry.
  kAppMenu = 0,

  // The content area context menu entry that is available when the user
  // right-clicks on any area of the page that doesn't contain text, links or
  // media. Only used on Desktop.
  kContentAreaContextMenuPage = 1,

  // The content area context menu entry that is available when the user
  // right-clicks on an image. Only used on Desktop.
  kContentAreaContextMenuImage = 2,

  // The pinned toolbar action button. Only used on Desktop.
  kToolbar = 3,

  // The find in page (Ctrl/Cmd-f) dialog button. Only used on Desktop.
  kFindInPage = 4,

  // The button in the omnibox (address bar).
  kOmnibox = 5,

  // The shutter button from LVF.
  kLVFShutterButton = 6,

  // The gallery picker from LVF.
  kLVFGallery = 7,

  // The context menu when long pressing a web image.
  kContextMenu = 8,

  // The Lens suggestion in the omnibox.
  kOmniboxPageAction = 9,

  // The contextual suggestions in the omnibox that take you directly to
  // contextual answers in the side panel.
  kOmniboxContextualSuggestion = 10,

  // The Lens homework action chip in the omnibox.
  kHomeworkActionChip = 11,

  // The Lens entry point in the AI hub menu.
  kAIHub = 12,

  // The Lens entry point in the Interactive Lens screen in the First Run
  // Experience.
  kFREPromo = 13,

  // The content area context menu entry that is available when the user
  // right-clicks on selected text. Only used on Desktop.
  kContentAreaContextMenuText = 14,

  // The content area context menu entry that is available when the user
  // right-clicks on a video frame. Only used on Desktop.
  kContentAreaContextMenuVideo = 15,

  // The compose or real box in the NTP realbox.
  kNtpContextualQuery = 16,

  // The compose flow in the omnibox.
  kOmniboxContextualQuery = 17,

  kMaxValue = kOmniboxContextualQuery
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensOverlayInvocationSource)
// When adding a value here, also update:
// - tools/metrics/histograms/metadata/lens/histograms.xml: <variants
// name="InvocationSources">
}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_INVOCATION_SOURCE_H_
