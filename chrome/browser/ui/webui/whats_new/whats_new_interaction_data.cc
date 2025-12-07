// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_interaction_data.h"

#include "chrome/browser/ui/webui/whats_new/whats_new.mojom-shared.h"
#include "content/public/browser/web_contents.h"

WhatsNewInteractionData::InteractionMetrics::InteractionMetrics() = default;
WhatsNewInteractionData::InteractionMetrics::~InteractionMetrics() = default;

WhatsNewInteractionData::WhatsNewInteractionData(
    content::WebContents* web_contents)
    : content::WebContentsUserData<WhatsNewInteractionData>(*web_contents) {}

WhatsNewInteractionData::~WhatsNewInteractionData() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(WhatsNewInteractionData);
