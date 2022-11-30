// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

/**
 * Interface to interact or communicate events on text links.
 */
public interface AssistantTextLinkDelegate {
    void onTextLinkClicked(int linkId);
}