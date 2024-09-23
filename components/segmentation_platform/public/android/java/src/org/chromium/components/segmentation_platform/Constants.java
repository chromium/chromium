// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

/** Stores constants related to segmentation models. */
public final class Constants {
    // Custom inputs for contextual page action's model. Model defined in
    // contextual_page_actions_model.cc.
    public static final String CONTEXTUAL_PAGE_ACTIONS_PRICE_TRACKING_INPUT = "can_track_price";
    public static final String CONTEXTUAL_PAGE_ACTIONS_READER_MODE_INPUT = "has_reader_mode";
    public static final String CONTEXTUAL_PAGE_ACTIONS_PRICE_INSIGHTS_INPUT = "has_price_insights";
    public static final String CONTEXTUAL_PAGE_ACTIONS_DISCOUNTS_INPUT = "has_discounts";
}
