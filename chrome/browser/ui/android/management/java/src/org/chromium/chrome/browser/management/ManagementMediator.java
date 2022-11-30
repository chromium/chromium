// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.management;

import android.content.Context;
import android.text.SpannableString;

import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * A mediator for the {@link ManagementCoordinator} responsible for handling business logic.
 */
public class ManagementMediator {
    private static final String LEARN_MORE_URL =
            "https://support.google.com/chrome/?p=is_chrome_managed";

    private final NativePageHost mHost;
    private final PropertyModel mModel;

    public ManagementMediator(NativePageHost host, Profile profile) {
        mHost = host;
        mModel = new PropertyModel.Builder(ManagementProperties.ALL_KEYS)
                         .with(ManagementProperties.BROWSER_IS_MANAGED,
                                 ManagedBrowserUtils.isBrowserManaged(profile))
                         .with(ManagementProperties.BROWSER_MANAGER_NAME,
                                 ManagedBrowserUtils.getBrowserManagerName(profile))
                         .with(ManagementProperties.LEARN_MORE_TEXT, getLearnMoreClickableText())
                         .build();
    }

    public PropertyModel getModel() {
        return mModel;
    }

    private SpannableString getLearnMoreClickableText() {
        final Context context = mHost.getContext();
        final NoUnderlineClickableSpan clickableLearnMoreSpan =
                new NoUnderlineClickableSpan(context, (v) -> { showHelpCenterArticle(); });
        return SpanApplier.applySpans(context.getString(R.string.management_learn_more),
                new SpanApplier.SpanInfo("<LINK>", "</LINK>", clickableLearnMoreSpan));
    }

    private void showHelpCenterArticle() {
        mHost.loadUrl(new LoadUrlParams(LEARN_MORE_URL), /*incognito=*/false);
    }
}
