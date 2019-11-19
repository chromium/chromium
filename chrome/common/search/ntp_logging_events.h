// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SEARCH_NTP_LOGGING_EVENTS_H_
#define CHROME_COMMON_SEARCH_NTP_LOGGING_EVENTS_H_

// The different types of events that are logged from the NTP. This enum is used
// to transfer information from the NTP javascript to the renderer and is *not*
// used as a UMA enum histogram's logged value.
// Note: Keep in sync with browser/resources/local_ntp/local_ntp.js, voice.js,
// most_visited_single.js, and custom_backgrounds.js.
enum NTPLoggingEventType {
  // Deleted: NTP_SERVER_SIDE_SUGGESTION = 0,
  // Deleted: NTP_CLIENT_SIDE_SUGGESTION = 1,
  // Deleted: NTP_TILE = 2,
  // Deleted: NTP_THUMBNAIL_TILE = 3,
  // Deleted: NTP_GRAY_TILE = 4,
  // Deleted: NTP_EXTERNAL_TILE = 5,
  // Deleted: NTP_THUMBNAIL_ERROR = 6,
  // Deleted: NTP_GRAY_TILE_FALLBACK = 7,
  // Deleted: NTP_EXTERNAL_TILE_FALLBACK = 8,
  // Deleted: NTP_MOUSEOVER = 9
  // Deleted: NTP_TILE_LOADED = 10,
  // Deleted: NTP_ALL_TILES_RECEIVED = 12,

  // All NTP tiles have finished loading (successfully or failing). Logged only
  // by the single-iframe version of the NTP.
  NTP_ALL_TILES_LOADED = 11,

  // Activated by clicking on the fakebox icon. Logged by Voice Search.
  NTP_VOICE_ACTION_ACTIVATE_FAKEBOX = 13,
  // Activated by keyboard shortcut.
  NTP_VOICE_ACTION_ACTIVATE_KEYBOARD = 14,
  // Close the voice overlay by a user's explicit action.
  NTP_VOICE_ACTION_CLOSE_OVERLAY = 15,
  // Submitted voice query.
  NTP_VOICE_ACTION_QUERY_SUBMITTED = 16,
  // Clicked on support link in error message.
  NTP_VOICE_ACTION_SUPPORT_LINK_CLICKED = 17,
  // Retried by clicking Try Again link.
  NTP_VOICE_ACTION_TRY_AGAIN_LINK = 18,
  // Retried by clicking microphone button.
  NTP_VOICE_ACTION_TRY_AGAIN_MIC_BUTTON = 19,
  // Errors received from the Speech Recognition API.
  NTP_VOICE_ERROR_NO_SPEECH = 20,
  NTP_VOICE_ERROR_ABORTED = 21,
  NTP_VOICE_ERROR_AUDIO_CAPTURE = 22,
  NTP_VOICE_ERROR_NETWORK = 23,
  NTP_VOICE_ERROR_NOT_ALLOWED = 24,
  NTP_VOICE_ERROR_SERVICE_NOT_ALLOWED = 25,
  NTP_VOICE_ERROR_BAD_GRAMMAR = 26,
  NTP_VOICE_ERROR_LANGUAGE_NOT_SUPPORTED = 27,
  NTP_VOICE_ERROR_NO_MATCH = 28,
  NTP_VOICE_ERROR_OTHER = 29,

  // A static Doodle was shown, coming from cache.
  NTP_STATIC_LOGO_SHOWN_FROM_CACHE = 30,
  // A static Doodle was shown, coming from the network.
  NTP_STATIC_LOGO_SHOWN_FRESH = 31,
  // A call-to-action Doodle image was shown, coming from cache.
  NTP_CTA_LOGO_SHOWN_FROM_CACHE = 32,
  // A call-to-action Doodle image was shown, coming from the network.
  NTP_CTA_LOGO_SHOWN_FRESH = 33,

  // A static Doodle was clicked.
  NTP_STATIC_LOGO_CLICKED = 34,
  // A call-to-action Doodle was clicked.
  NTP_CTA_LOGO_CLICKED = 35,
  // An animated Doodle was clicked.
  NTP_ANIMATED_LOGO_CLICKED = 36,

  // The One Google Bar was shown.
  NTP_ONE_GOOGLE_BAR_SHOWN = 37,

  // The NTP background has been customized with an image.
  NTP_BACKGROUND_CUSTOMIZED = 38,
  // Shortcuts have been customized on the NTP.
  NTP_SHORTCUT_CUSTOMIZED = 39,

  // The 'Chrome backgrounds' menu item was clicked.
  NTP_CUSTOMIZE_CHROME_BACKGROUNDS_CLICKED = 40,
  // The 'Upload an image' menu item was clicked.
  NTP_CUSTOMIZE_LOCAL_IMAGE_CLICKED = 41,
  // The 'Restore default background' menu item was clicked.
  NTP_CUSTOMIZE_RESTORE_BACKGROUND_CLICKED = 42,
  // The attribution link on a customized background image was clicked.
  NTP_CUSTOMIZE_ATTRIBUTION_CLICKED = 43,
  // The 'Add shortcut' link was clicked.
  NTP_CUSTOMIZE_ADD_SHORTCUT_CLICKED = 44,
  // The 'Edit shortcut' link was clicked.
  NTP_CUSTOMIZE_EDIT_SHORTCUT_CLICKED = 45,
  // The 'Restore default shortcuts' menu item was clicked.
  NTP_CUSTOMIZE_RESTORE_SHORTCUTS_CLICKED = 46,

  // A collection was selected in the 'Chrome backgrounds' dialog.
  NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_COLLECTION = 47,
  // An image was selected in the 'Chrome backgrounds' dialog.
  NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_IMAGE = 48,
  // 'Cancel' was clicked in the 'Chrome backgrounds' dialog.
  NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL = 49,
  // 'Done' was clicked in the 'Chrome backgrounds' dialog.
  NTP_CUSTOMIZE_CHROME_BACKGROUND_DONE = 50,

  // 'Cancel' was clicked in the 'Upload an image' dialog.
  NTP_CUSTOMIZE_LOCAL_IMAGE_CANCEL = 51,
  // 'Done' was clicked in the 'Upload an image' dialog.
  NTP_CUSTOMIZE_LOCAL_IMAGE_DONE = 52,

  // A custom shortcut was removed.
  NTP_CUSTOMIZE_SHORTCUT_REMOVE = 53,
  // 'Cancel' was clicked in the 'Edit shortcut' dialog.
  NTP_CUSTOMIZE_SHORTCUT_CANCEL = 54,
  // 'Done' was clicked in the 'Edit shortcut' dialog.
  NTP_CUSTOMIZE_SHORTCUT_DONE = 55,
  // A custom shortcut action was undone.
  NTP_CUSTOMIZE_SHORTCUT_UNDO = 56,
  // All custom shortcuts were restored.
  NTP_CUSTOMIZE_SHORTCUT_RESTORE_ALL = 57,
  // A custom shortcut was added.
  NTP_CUSTOMIZE_SHORTCUT_ADD = 58,
  // A custom shortcut was updated.
  NTP_CUSTOMIZE_SHORTCUT_UPDATE = 59,

  // A middle slot promo was shown.
  NTP_MIDDLE_SLOT_PROMO_SHOWN = 60,
  // A promo link was clicked.
  NTP_MIDDLE_SLOT_PROMO_LINK_CLICKED = 61,

  // The shortcut type displayed (i.e. Most Visited or custom links) was
  // changed.
  NTP_CUSTOMIZE_SHORTCUT_TOGGLE_TYPE = 62,
  // The visibility of shortcuts was changed.
  NTP_CUSTOMIZE_SHORTCUT_TOGGLE_VISIBILITY = 63,

  // The richer picker was opened.
  NTP_CUSTOMIZATION_MENU_OPENED = 64,
  // 'Cancel' was clicked in the richer picker.
  NTP_CUSTOMIZATION_MENU_CANCEL = 65,
  // 'Done' was clicked in the richer picker.
  NTP_CUSTOMIZATION_MENU_DONE = 66,

  // 'Upload from device' was selected in the richer picker.
  NTP_BACKGROUND_UPLOAD_FROM_DEVICE = 67,
  // A collection tile was selected in the richer picker.
  NTP_BACKGROUND_OPEN_COLLECTION = 68,
  // A image tile was selected in the richer picker.
  NTP_BACKGROUND_SELECT_IMAGE = 69,
  // An image was set as the NTP background.
  NTP_BACKGROUND_IMAGE_SET = 71,
  // The back arrow was clicked in the richer picker.
  NTP_BACKGROUND_BACK_CLICK = 72,
  // The 'No background' tile was selected in the richer picker.
  NTP_BACKGROUND_DEFAULT_SELECTED = 73,
  // 'Cancel' was clicked in the image selection dialog.
  NTP_BACKGROUND_UPLOAD_CANCEL = 75,
  // 'Done' was clicked in the image selection dialog.
  NTP_BACKGROUND_UPLOAD_DONE = 76,
  // The NTP background image was reset in the richer picker.
  NTP_BACKGROUND_IMAGE_RESET = 77,

  // The 'My shortcuts' (i.e. custom links) option was clicked in the richer
  // picker.
  NTP_CUSTOMIZE_SHORTCUT_CUSTOM_LINKS_CLICKED = 78,
  // The 'Most visited sites' option was clicked in the richer picker.
  NTP_CUSTOMIZE_SHORTCUT_MOST_VISITED_CLICKED = 79,
  // The 'Hide shortcuts' toggle was clicked in the richer picker.
  NTP_CUSTOMIZE_SHORTCUT_VISIBILITY_TOGGLE_CLICKED = 80,

  // The 'refresh daily' toggle was licked in the richer picker.
  NTP_BACKGROUND_REFRESH_TOGGLE_CLICKED = 81,
  // Daily refresh was enabled by clicked 'Done' in the richer picker.
  NTP_BACKGROUND_DAILY_REFRESH_ENABLED = 82,

  NTP_EVENT_TYPE_LAST = NTP_BACKGROUND_DAILY_REFRESH_ENABLED
};

// The different types of events that are logged for NTP search suggestions,
// such as number of chips shown and the index of chips that are clicked. This
// enum is used to transfer information from the NTP javascript to the renderer
// and is *not* used as a UMA enum histogram's logged value. These events may be
// logged by javascript served from GWS, see
// google3/java/com/google/gws/plugins/newtab/suggestions.js.
enum class NTPSuggestionsLoggingEventType {
  kShownCount = 0,
  kIndexClicked = 1,
  kMaxValue = kIndexClicked,
};

#endif  // CHROME_COMMON_SEARCH_NTP_LOGGING_EVENTS_H_
