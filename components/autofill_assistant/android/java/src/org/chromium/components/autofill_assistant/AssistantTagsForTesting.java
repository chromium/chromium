// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

/**
 * Contains tags used for java UI tests.
 *
 * These tags are used to tag dynamically created views (i.e., without resource ID) to allow tests
 * to easily retrieve and test them.
 */
public class AssistantTagsForTesting {
    public static final String COLLECT_USER_DATA_ACCORDION_TAG = "accordion";
    public static final String COLLECT_USER_DATA_LOGIN_SECTION_TAG = "login";
    public static final String COLLECT_USER_DATA_CONTACT_DETAILS_SECTION_TAG = "contact";
    public static final String COLLECT_USER_DATA_PHONE_NUMBER_SECTION_TAG = "phone_number";
    public static final String COLLECT_USER_DATA_PAYMENT_METHOD_SECTION_TAG = "payment";
    public static final String COLLECT_USER_DATA_SHIPPING_ADDRESS_SECTION_TAG = "shipping";
    public static final String COLLECT_USER_DATA_RADIO_TERMS_SECTION_TAG = "radio_terms";
    public static final String COLLECT_USER_DATA_CHECKBOX_TERMS_SECTION_TAG = "checkbox_terms";
    public static final String COLLECT_USER_DATA_INFO_SECTION_TAG = "info_section";
    public static final String COLLECT_USER_DATA_TERMS_REQUIRE_REVIEW = "require_review";
    public static final String COLLECT_USER_DATA_DATA_ORIGIN_NOTICE_TAG = "data_origin_notice";
    public static final String COLLECT_USER_DATA_SUMMARY_LOADING_SPINNER_TAG =
            "summary_loading_spinner";
    public static final String VERTICAL_EXPANDER_CHEVRON = "chevron";
    public static final String COLLECT_USER_DATA_CHOICE_LIST = "choicelist";
    public static final String CHOICE_LIST_EDIT_ICON = "edit_icon";
    public static final String RECYCLER_VIEW_TAG = "recycler_view";
    public static final String PROGRESSBAR_ICON_TAG = "progress_icon_%d";
    public static final String PROGRESSBAR_LINE_TAG = "progress_line_%d";
    public static final String PROGRESSBAR_LINE_FOREGROUND_TAG = "progress_line_foreground";
    public static final String TTS_ENABLED_ICON_TAG = "tts_enabled_icon";
    public static final String TTS_PLAYING_ICON_TAG = "tts_playing_icon";
    public static final String TTS_DISABLED_ICON_TAG = "tts_disabled_icon";
}
