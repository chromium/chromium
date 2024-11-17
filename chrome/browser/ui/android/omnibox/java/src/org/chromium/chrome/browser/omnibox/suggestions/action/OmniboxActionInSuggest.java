// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import android.content.Intent;
import android.util.SparseArray;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.components.omnibox.EntityInfoProto;
import org.chromium.components.omnibox.R;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;

import java.net.URISyntaxException;

/** Omnibox action for showing the Action in Suggest UI. */
public class OmniboxActionInSuggest extends OmniboxAction {
    /** Map of {@link EntityInfoProto.ActionInfo.ActionType} to {@link ChipIcon}. */
    private static final SparseArray<ChipIcon> ICON_MAP = createIconMap();

    /** The details about the underlying action. */
    public final /* EntityInfoProto.ActionInfo.ActionType */ int actionType;

    private final @NonNull String mActionUri;

    public OmniboxActionInSuggest(
            long nativeInstance,
            @NonNull String hint,
            @NonNull String accessibilityHint,
            /* EntityInfoProto.ActionInfo.ActionType */ int actionType,
            @NonNull String actionUri) {
        super(
                OmniboxActionId.ACTION_IN_SUGGEST,
                nativeInstance,
                hint,
                accessibilityHint,
                ICON_MAP.get(actionType, DEFAULT_ICON),
                R.style.TextAppearance_ChipText);
        this.actionType = actionType;
        mActionUri = actionUri;
    }

    /**
     * Cast supplied OmniboxAction to OmniboxActionInSuggest. Requires the supplied input to be a
     * valid instance of an OmniboxActionInSuggest whose actionId is the ACTION_IN_SUGGEST.
     */
    public static @NonNull OmniboxActionInSuggest from(@NonNull OmniboxAction action) {
        assert action != null;
        assert action.actionId == OmniboxActionId.ACTION_IN_SUGGEST;
        assert action instanceof OmniboxActionInSuggest;
        return (OmniboxActionInSuggest) action;
    }

    /** Returns a map of ActionType to ChipIcon. */
    private static SparseArray<ChipIcon> createIconMap() {
        var map = new SparseArray<ChipIcon>();
        map.put(
                EntityInfoProto.ActionInfo.ActionType.CALL_VALUE,
                new ChipIcon(R.drawable.action_call, true));
        map.put(
                EntityInfoProto.ActionInfo.ActionType.DIRECTIONS_VALUE,
                new ChipIcon(R.drawable.action_directions, true));
        map.put(
                EntityInfoProto.ActionInfo.ActionType.REVIEWS_VALUE,
                new ChipIcon(R.drawable.action_reviews, true));
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
        }

        switch (actionType) {
            case EntityInfoProto.ActionInfo.ActionType.REVIEWS_VALUE:
                delegate.loadPageInCurrentTab(intent.getDataString());
                actionStarted = true;
                break;

            case EntityInfoProto.ActionInfo.ActionType.CALL_VALUE:
                // Don't call directly. Use `DIAL` instead to let the user decide.
                // Note also that ACTION_CALL requires a dedicated permission.
                intent.setAction(Intent.ACTION_DIAL);
                // Start dialer even if the user is in incognito mode. The intent only pre-dials
                // the phone number without ever making the call. This gives the user the chance
                // to abandon before making a call.
                actionStarted = delegate.startActivity(intent);
                break;

            case EntityInfoProto.ActionInfo.ActionType.DIRECTIONS_VALUE:
                // Open directions in maps only if maps are installed and the incognito mode is
                // not engaged. In all other cases, redirect the action to Browser.
                if (!isIncognito) {
                    actionStarted = delegate.startActivity(intent);
                }
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

            if (actionType == EntityInfoProto.ActionInfo.ActionType.DIRECTIONS_VALUE) {
                delegate.loadPageInCurrentTab(intent.getDataString());
            }
        }
    }
}
