// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

/**
 * Interface for obscuring tabs hiding them from the accessibility tree.
 */
public interface AssistantTabObscuringUtil {
    /**
     * Notify the system that there is a feature obscuring all visible tabs for accessibility. Hides
     * all tabs from the accessibility tree.
     */
    void obscureAllTabs();
    /**
     *  Unobscures the content of all tabs. Note that other parts of the system might still hold the
     *  tabs obscured.
     */
    void unobscureAllTabs();
}
