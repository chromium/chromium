// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.management;

import android.content.Context;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;

import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.ChromeBulletSpan;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** A mediator for the {@link ManagementCoordinator} responsible for handling business logic. */
public class ManagementMediator {
    private static final String CHROME_MANAGED_LEARN_MORE_URL =
            "https://support.google.com/chrome/?p=is_chrome_managed";
    private static final String PROFILE_REPORTING_LEARN_MORE_URL =
            "https://support.google.com/chrome/a/?p=browser_profile_details";

    private final NativePageHost mHost;
    private final PropertyModel mModel;

    public ManagementMediator(NativePageHost host, Profile profile) {
        mHost = host;
        mModel =
                new PropertyModel.Builder(ManagementProperties.ALL_KEYS)
                        .with(
                                ManagementProperties.BROWSER_IS_MANAGED,
                                ManagedBrowserUtils.isBrowserManaged(profile))
                        .with(
                                ManagementProperties.PROFILE_IS_MANAGED,
                                ManagedBrowserUtils.isProfileManaged(profile))
                        .with(ManagementProperties.TITLE, ManagedBrowserUtils.getTitle(profile))
                        .with(
                                ManagementProperties.LEARN_MORE_TEXT,
                                getLearnMoreClickableText(CHROME_MANAGED_LEARN_MORE_URL))
                        .with(
                                ManagementProperties.BROWSER_REPORTING_IS_ENABLED,
                                ManagedBrowserUtils.isBrowserReportingEnabled())
                        .with(
                                ManagementProperties.PROFILE_REPORTING_IS_ENABLED,
                                ManagedBrowserUtils.isProfileReportingEnabled(profile))
                        .with(
                                ManagementProperties.PROFILE_REPORTING_TEXT,
                                getProfileReportingText())
                        .with(
                                ManagementProperties.LEGACY_TECH_REPORTING_IS_ENABLED,
                                isLegacyTechReportingEnabled(UserPrefs.get(profile)))
                        .with(
                                ManagementProperties.LEGACY_TECH_REPORTING_TEXT,
                                getLegacyTechReportingClickableText())
                        .build();
    }

    public PropertyModel getModel() {
        return mModel;
    }

    private SpannableString getLearnMoreClickableText(String url) {
        final Context context = mHost.getContext();
        final NoUnderlineClickableSpan clickableLearnMoreSpan =
                new NoUnderlineClickableSpan(
                        context,
                        (v) -> {
                            showHelpCenterArticle(url);
                        });
        return SpanApplier.applySpans(
                context.getString(R.string.management_learn_more),
                new SpanApplier.SpanInfo("<LINK>", "</LINK>", clickableLearnMoreSpan));
    }

    private SpannableString buildBulletString(int stringResId) {
        SpannableString bullet = new SpannableString(mHost.getContext().getString(stringResId));
        bullet.setSpan(new ChromeBulletSpan(mHost.getContext()), 0, bullet.length(), 0);
        return bullet;
    }

    private SpannableStringBuilder getProfileReportingText() {
        SpannableStringBuilder spannableString = new SpannableStringBuilder();
        spannableString
                .append(buildBulletString(R.string.management_profile_reporting_overview))
                .append("\n")
                .append(buildBulletString(R.string.management_profile_reporting_username))
                .append("\n")
                .append(buildBulletString(R.string.management_profile_reporting_browser))
                .append("\n")
                .append(buildBulletString(R.string.management_profile_reporting_policy))
                .append("\n");

        SpannableString learn_more_link =
                getLearnMoreClickableText(PROFILE_REPORTING_LEARN_MORE_URL);
        learn_more_link.setSpan(
                new ChromeBulletSpan(mHost.getContext()), 0, learn_more_link.length(), 0);
        spannableString.append(learn_more_link);

        return spannableString;
    }

    private boolean isLegacyTechReportingEnabled(PrefService prefs) {
        return prefs.isManagedPreference(Pref.CLOUD_LEGACY_TECH_REPORT_ALLOWLIST);
    }

    private SpannableString getLegacyTechReportingClickableText() {
        Context context = mHost.getContext();

        // The `text` here is a string with HTML style hyberlink that is used by webui with
        // following format:
        //   ...text <a href="https://url">text</a> text...
        // Convert it to Android View with exact same function.
        String text = context.getString(R.string.management_legacy_tech_report);

        Matcher matcher = Pattern.compile("href=\"(.*?)\"").matcher(text);
        if (!matcher.find()) {
            assert false;
            return new SpannableString(text);
        }

        NoUnderlineClickableSpan linkSpan =
                new NoUnderlineClickableSpan(
                        context,
                        v -> {
                            mHost.loadUrl(
                                    new LoadUrlParams(matcher.group(1)), /* incognito= */ false);
                        });

        return SpanApplier.applySpans(
                text.replaceFirst("(.*)<a.*>(.*)</a>(.*)", "$1<link>$2</link>$3"),
                new SpanApplier.SpanInfo("<link>", "</link>", linkSpan));
    }

    private void showHelpCenterArticle(String url) {
        mHost.loadUrl(new LoadUrlParams(url), /* incognito= */ false);
    }
}
