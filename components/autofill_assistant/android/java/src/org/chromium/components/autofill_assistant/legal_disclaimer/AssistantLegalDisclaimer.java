// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.legal_disclaimer;

import androidx.annotation.NonNull;

public class AssistantLegalDisclaimer {
    private final String mMessage;

    private final AssistantLegalDisclaimerDelegate mDelegate;

    public AssistantLegalDisclaimer(
            @NonNull AssistantLegalDisclaimerDelegate delegate, @NonNull String message) {
        this.mMessage = message;
        this.mDelegate = delegate;
    }

    @NonNull
    String getMessage() {
        return mMessage;
    }

    @NonNull
    AssistantLegalDisclaimerDelegate getDelegate() {
        return mDelegate;
    }
}
