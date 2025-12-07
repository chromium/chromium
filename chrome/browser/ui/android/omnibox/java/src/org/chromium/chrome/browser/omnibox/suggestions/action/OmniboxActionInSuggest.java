// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Intent;
import android.util.SparseArray;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.components.omnibox.R;
import org.chromium.components.omnibox.SuggestTemplateInfoProto.SuggestTemplateInfo;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.net.URISyntaxException;

/** Omnibox action for showing the Action in Suggest UI. */
@NullMarked
public class OmniboxActionInSuggest extends OmniboxAction {
    /** Map of {@link SuggestTemplateInfo.TemplateAction.ActionType} to {@link ActionIcon}. */
    private static final SparseArray<ActionIcon> ICON_MAP = createIconMap();

    /** The details about the underlying action. */
    public final /* SuggestTemplateInfo.TemplateAction.ActionType */ int actionType;

    public final int tabId;

    private final String mActionUri;

    public OmniboxActionInSuggest(
            long nativeInstance,
            String hint,
            String accessibilityHint,
            /* SuggestTemplateInfo.TemplateAction.ActionType */ int actionType,
            String actionUri,
            int tabId,
            boolean showAsActionButton) {
        super(
                OmniboxActionId.ACTION_IN_SUGGEST,
                nativeInstance,
                hint,
                accessibilityHint,
                ICON_MAP.get(actionType, DEFAULT_ICON),
                R.style.TextAppearance_ChipText,
                showAsActionButton,
                actionType == SuggestTemplateInfo.TemplateAction.ActionType.CHROME_TAB_SWITCH_VALUE
                        ? WindowOpenDisposition.SWITCH_TO_TAB
                        : WindowOpenDisposition.CURRENT_TAB);
        this.actionType = actionType;
        this.tabId = tabId;
        mActionUri = actionUri;
    }

    /**
     * Cast supplied OmniboxAction to OmniboxActionInSuggest. Requires the supplied input to be a
     * valid instance of an OmniboxActionInSuggest whose actionId is the ACTION_IN_SUGGEST.
     */
    public static OmniboxActionInSuggest from(OmniboxAction action) {
        assert action != null;
        assert action.actionId == OmniboxActionId.ACTION_IN_SUGGEST;
        assert action instanceof OmniboxActionInSuggest;
        return (OmniboxActionInSuggest) action;
    }

    /** Returns a map of ActionType to ActionIcon. */
    private static SparseArray<ActionIcon> createIconMap() {
        var map = new SparseArray<ActionIcon>();
        map.put(
                SuggestTemplateInfo.TemplateAction.ActionType.CALL_VALUE,
                new ActionIcon(R.drawable.action_call, true));
        map.put(
                SuggestTemplateInfo.TemplateAction.ActionType.DIRECTIONS_VALUE,
                new ActionIcon(R.drawable.action_directions, true));
        map.put(
                SuggestTemplateInfo.TemplateAction.ActionType.REVIEWS_VALUE,
                new ActionIcon(R.drawable.action_reviews, true));
        map.put(
                SuggestTemplateInfo.TemplateAction.ActionType.CHROME_AIM_VALUE,
                new ActionIcon(
                        org.chromium.chrome.browser.omnibox.R.drawable.search_spark_rainbow,
                        org.chromium.chrome.browser.omnibox.R.drawable.search_spark_rainbow,
                        org.chromium.chrome.browser.omnibox.R.drawable
                                .search_spark_rainbow_incognito,
                        false));
        map.put(
                SuggestTemplateInfo.TemplateAction.ActionType.CHROME_TAB_SWITCH_VALUE,
                new ActionIcon(
                        org.chromium.chrome.browser.omnibox.R.drawable.tab,
                        org.chromium.chrome.browser.omnibox.R.drawable.switch_to_tab,
                        org.chromium.chrome.browser.omnibox.R.drawable.switch_to_tab,
                        true));
        return map;
    }

    /** Execute an Intent associated with OmniboxActionInSuggest. */
    @Override
    public void execute(OmniboxActionDelegate delegate) {
        boolean actionStarted = false;
        boolean isIncognito = delegate.isIncognito();
        Intent intent = null;

        try {
            intent = Intent.parseUri(mActionUri, Intent.URI_INTENT_SCHEME);
        } catch (URISyntaxException e) {
            // Never happens. http://b/279756377.
            return;
        }

        switch (actionType) {
            case SuggestTemplateInfo.TemplateAction.ActionType.REVIEWS_VALUE:
            case SuggestTemplateInfo.TemplateAction.ActionType.CHROME_AIM_VALUE:
                delegate.loadPageInCurrentTab(assumeNonNull(intent.getDataString()));
                actionStarted = true;
                break;

            case SuggestTemplateInfo.TemplateAction.ActionType.CALL_VALUE:
                // Don't call directly. Use `DIAL` instead to let the user decide.
                // Note also that ACTION_CALL requires a dedicated permission.
                intent.setAction(Intent.ACTION_DIAL);
                // Start dialer even if the user is in incognito mode. The intent only pre-dials
                // the phone number without ever making the call. This gives the user the chance
                // to abandon before making a call.
                actionStarted = delegate.startActivity(intent);
                break;

            case SuggestTemplateInfo.TemplateAction.ActionType.DIRECTIONS_VALUE:
                // Open directions in maps only if maps are installed and the incognito mode is
                // not engaged. In all other cases, redirect the action to Browser.
                if (!isIncognito) {
                    actionStarted = delegate.startActivity(intent);
                }
                break;

            case SuggestTemplateInfo.TemplateAction.ActionType.CHROME_TAB_SWITCH_VALUE:
                if (!delegate.switchToTab(tabId, new GURL(mActionUri))) {
                    delegate.loadPageInCurrentTab(assumeNonNull(intent.getDataString()));
                }
                actionStarted = true;
                break;

                // No `default` to capture new variants.
        }

        // Record intent started only if it was sent.
        if (actionStarted) {
            OmniboxMetrics.recordActionInSuggestIntentResult(
                    OmniboxMetrics.ActionInSuggestIntentResult.SUCCESS);
        } else {
            // At this point we know that we were either unable to launch the target activity
            // or the user is browsing incognito, where we suppress some actions.
            // We may still be able to handle the corresponding action inside the browser.
            if (!isIncognito) {
                OmniboxMetrics.recordActionInSuggestIntentResult(
                        OmniboxMetrics.ActionInSuggestIntentResult.ACTIVITY_NOT_FOUND);
            }

            if (actionType == SuggestTemplateInfo.TemplateAction.ActionType.DIRECTIONS_VALUE) {
                delegate.loadPageInCurrentTab(assumeNonNull(intent.getDataString()));
            }
        }
    }
}
