// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill.data;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({CardUnmaskChallengeOptionType.UnknownType, CardUnmaskChallengeOptionType.SmsOtp,
        CardUnmaskChallengeOptionType.MaxValue})
@Retention(RetentionPolicy.SOURCE)
public @interface CardUnmaskChallengeOptionType {
    int UnknownType = 0;
    int SmsOtp = 1;
    int MaxValue = 1;
}
