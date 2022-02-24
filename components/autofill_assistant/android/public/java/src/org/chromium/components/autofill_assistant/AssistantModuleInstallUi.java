// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import org.chromium.base.Consumer;

/**
 * UI informing the user about the status of installing a dynamic feature module.
 */
public interface AssistantModuleInstallUi {
    /**
     * Used to create {@link AssistantModuleInstallUi}.
     */
    public interface Provider {
        /**
         * Creates a {@link AssistantModuleInstallUi}.
         */
        AssistantModuleInstallUi create(Consumer<Boolean> onFailure);
    }

    /**
     * Show UI indicating the start of a module install.
     */
    public void showInstallStartUi();

    /**
     * Show UI indicating the failure of a module install.
     */
    public void showInstallFailureUi();
}
