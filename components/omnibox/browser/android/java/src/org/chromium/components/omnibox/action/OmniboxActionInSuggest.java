// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import android.content.Intent;
import android.util.SparseArray;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.omnibox.EntityInfoProto;
import org.chromium.components.omnibox.OmniboxMetrics;
import org.chromium.components.omnibox.R;

import java.net.URISyntaxException;

/**
 * Omnibox action for showing the Action in Suggest UI.
 */
public class OmniboxActionInSuggest extends OmniboxAction {
    /** Map of {@link EntityInfoProto.ActionInfo.ActionType} to {@link ChipIcon}. */
    private static final SparseArray<ChipIcon> ICON_MAP = createIconMap();
    /** The details about the underlying action. */
    public final @NonNull EntityInfoProto.ActionInfo actionInfo;

    public OmniboxActionInSuggest(
            @NonNull String hint, @NonNull EntityInfoProto.ActionInfo actionInfo) {
        super(OmniboxActionType.ACTION_IN_SUGGEST, hint,
                ICON_MAP.get(actionInfo.getActionType().getNumber(), null));
        this.actionInfo = actionInfo;
    }

    /**
     * Cast supplied OmniboxAction to OmniboxActionInSuggest.
     * Requires the supplied input to be a valid instance of an OmniboxActionInSuggest whose
     * actionId is the ACTION_IN_SUGGEST.
     */
    public static @NonNull OmniboxActionInSuggest from(@NonNull OmniboxAction action) {
        assert action != null;
        assert action.actionId == OmniboxActionType.ACTION_IN_SUGGEST;
        assert action instanceof OmniboxActionInSuggest;
        return (OmniboxActionInSuggest) action;
    }

    /** Returns a map of ActionType to ChipIcon. */
    private static SparseArray<ChipIcon> createIconMap() {
        var map = new SparseArray<ChipIcon>();
        map.put(EntityInfoProto.ActionInfo.ActionType.CALL_VALUE,
                new ChipIcon(R.drawable.action_call, true));
        map.put(EntityInfoProto.ActionInfo.ActionType.DIRECTIONS_VALUE,
                new ChipIcon(R.drawable.action_directions, true));
        map.put(EntityInfoProto.ActionInfo.ActionType.WEBSITE_VALUE,
                new ChipIcon(R.drawable.action_web, true));
        return map;
    }

    /**
     * Execute an Intent associated with OmniboxActionInSuggest.
     *
     * TODO(crbug/1418077): pass the dependencies to constructor and define method in the interface.
     *
     * @param loadPageInCurrentTab loads the page in the current tab (if available), else new tab
     * @param startActivity starts the activity described by supplied intent
     */
    @Override
    public void execute(OmniboxActionDelegate delegate) {
        var actionType = actionInfo.getActionType();
        boolean actionStarted = false;
        boolean isIncognito = delegate.isIncognito();
        Intent intent = null;

        try {
            intent = Intent.parseUri(actionInfo.getActionUri(), Intent.URI_INTENT_SCHEME);
        } catch (URISyntaxException e) {
            // Never happens. http://b/279756377.
        }

        switch (actionType) {
            case WEBSITE:
                // Rather than invoking an intent that opens a new tab, load the page in the
                // current tab.
                delegate.loadPageInCurrentTab(intent.getDataString());
                actionStarted = true;
                break;

            case REVIEWS:
                assert false : "Pending implementation";
                break;

            case CALL:
                // Don't call directly. Use `DIAL` instead to let the user decide.
                // Note also that ACTION_CALL requires a dedicated permission.
                intent.setAction(Intent.ACTION_DIAL);
                // Start dialer even if the user is in incognito mode. The intent only pre-dials
                // the phone number without ever making the call. This gives the user the chance
                // to abandon before making a call.
                actionStarted = delegate.startActivity(intent);
                break;

            case DIRECTIONS:
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

            switch (actionType) {
                case DIRECTIONS:
                    delegate.loadPageInCurrentTab(intent.getDataString());
                    break;

                case CALL:
                case REVIEWS:
                case WEBSITE:
                    // Give up. Don't add the `default` clause though, capture missed variants.
                    break;
            }
        }
    }

    @CalledByNative
    @VisibleForTesting
    public static @Nullable OmniboxActionInSuggest build(
            @NonNull String hint, @NonNull byte[] serializedActionInfo) {
        try {
            return new OmniboxActionInSuggest(
                    hint, EntityInfoProto.ActionInfo.parseFrom(serializedActionInfo));
        } catch (InvalidProtocolBufferException e) {
        }
        return null;
    }
}
