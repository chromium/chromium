// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.AccessibilityUtil;
/**
 * Generic static dependencies interface. The concrete implementation will depend on the browser
 * framework, i.e., WebLayer vs. Chrome.
 */
@JNINamespace("autofill_assistant")
public interface AssistantStaticDependencies {
    @CalledByNative
    long createNative();

    /**
     * Create the Activity specific dependencies.
     * */
    AssistantDependencies createDependencies(Activity activity);

    AccessibilityUtil getAccessibilityUtil();

    @CalledByNative
    default boolean isAccessibilityEnabled() {
        return getAccessibilityUtil().isAccessibilityEnabled();
    }

    /**
     * Returns a utility for obscuring all tabs. NOTE: Each call returns a new instance that can
     * only unobscure what it obscured!
     */
    @Nullable
    AssistantTabObscuringUtil getTabObscuringUtilOrNull(WindowAndroid windowAndroid);

    @CalledByNative
    AssistantInfoPageUtil createInfoPageUtil();

    AssistantFeedbackUtil createFeedbackUtil();

    AssistantTabUtil createTabUtil();

    AssistantSettingsUtil createSettingsUtil();

    @CalledByNative
    AssistantAccessTokenUtil createAccessTokenUtil();

    BrowserContextHandle getBrowserContext();

    @CalledByNative
    ImageFetcher createImageFetcher();

    @CalledByNative
    LargeIconBridge createIconBridge();

    @Nullable
    AssistantProfileImageUtil createProfileImageUtilOrNull(
            Context context, @DimenRes int imageSizeRedId);

    @Nullable
    AssistantEditorFactory createEditorFactory();
}
