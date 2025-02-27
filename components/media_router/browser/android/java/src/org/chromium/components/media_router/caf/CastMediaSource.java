// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf;

import android.net.Uri;

import androidx.annotation.Nullable;
import androidx.mediarouter.media.MediaRouteSelector;

import com.google.android.gms.cast.CastMediaControlIntent;

import org.chromium.components.media_router.MediaSource;

import java.util.Arrays;
import java.util.List;

/** Abstracts parsing the Cast application id and other parameters from the source ID. */
public class CastMediaSource implements MediaSource {
    public static final String AUTOJOIN_CUSTOM_CONTROLLER_SCOPED = "custom_controller_scoped";
    public static final String AUTOJOIN_TAB_AND_ORIGIN_SCOPED = "tab_and_origin_scoped";
    public static final String AUTOJOIN_ORIGIN_SCOPED = "origin_scoped";
    public static final String AUTOJOIN_PAGE_SCOPED = "page_scoped";
    private static final List<String> AUTOJOIN_POLICIES =
            Arrays.asList(
                    AUTOJOIN_CUSTOM_CONTROLLER_SCOPED,
                    AUTOJOIN_TAB_AND_ORIGIN_SCOPED,
                    AUTOJOIN_ORIGIN_SCOPED,
                    AUTOJOIN_PAGE_SCOPED);

    private static final String CAST_SOURCE_ID_SEPARATOR = "/";
    private static final String CAST_SOURCE_ID_APPLICATION_ID = "__castAppId__";
    private static final String CAST_SOURCE_ID_CLIENT_ID = "__castClientId__";
    private static final String CAST_SOURCE_ID_AUTOJOIN_POLICY = "__castAutoJoinPolicy__";
    private static final String CAST_APP_CAPABILITIES_PREFIX = "(";
    private static final String CAST_APP_CAPABILITIES_SUFFIX = ")";
    private static final String CAST_APP_CAPABILITIES_SEPARATOR = ",";
    private static final List<String> CAST_APP_CAPABILITIES =
            Arrays.asList("video_out", "audio_out", "video_in", "audio_in", "multizone_group");

    /** The protocol for Cast Presentation URLs. */
    private static final String CAST_URL_PROTOCOL = "cast:";

    /** The query parameter key for Cast Client ID in a Cast Presentation URL. */
    private static final String CAST_URL_CLIENT_ID = "clientId";

    /** The query parameter key for autojoin policy in a Cast Presentation URL. */
    private static final String CAST_URL_AUTOJOIN_POLICY = "autoJoinPolicy";

    /** The query parameter key for app capabilities in a Cast Presentation URL. */
    private static final String CAST_URL_CAPABILITIES = "capabilities";

    /** The original presentation URL that the {@link CastMediaSource} object was created from. */
    private final String mSourceId;

    /**
     * The Cast application id, can be invalid in which case {@link CastMediaRouteProvider}
     * will explicitly report no sinks available.
     */
    private final String mApplicationId;

    /**
     * A numeric identifier for the Cast Web SDK, unique for the frame providing the
     * presentation URL. Can be null.
     */
    private final String mClientId;

    /**
     * Defines Cast-specific behavior for {@link CastMediaRouteProvider#joinRoute}. Defaults to
     * {@link CastMediaSource#AUTOJOIN_TAB_AND_ORIGIN_SCOPED}.
     */
    private final String mAutoJoinPolicy;

    /** Defines the capabilities of the particular application id. Can be null. */
    private final String[] mCapabilities;

    /**
     * Initializes the media source from the source id.
     * @param sourceId the source id for the Cast media source (a presentation url).
     * @return an initialized media source if the id is valid, null otherwise.
     */
    @Nullable
    public static CastMediaSource from(String sourceId) {
        assert sourceId != null;
        return sourceId.startsWith(CAST_URL_PROTOCOL)
                ? fromCastUrl(sourceId)
                : fromLegacyUrl(sourceId);
    }

    /**
     * Returns a new {@link MediaRouteSelector} to use for Cast device filtering for this
     * particular media source or null if the application id is invalid.
     *
     * @return an initialized route selector or null.
     */
    @Override
    public MediaRouteSelector buildRouteSelector() {
        try {
            return new MediaRouteSelector.Builder()
                    .addControlCategory(CastMediaControlIntent.categoryForCast(mApplicationId))
                    .build();
        } catch (IllegalArgumentException e) {
            return null;
        }
    }

    /** @return the Cast application id corresponding to the source. */
    @Override
    public String getApplicationId() {
        return mApplicationId;
    }

    /** @return the client id if passed in the source id. Can be null. */
    @Nullable
    public String getClientId() {
        return mClientId;
    }

    /** @return the auto join policy which must be one of the AUTOJOIN constants defined above. */
    public String getAutoJoinPolicy() {
        return mAutoJoinPolicy;
    }

    /** @return the id identifying the media source */
    @Override
    public String getSourceId() {
        return mSourceId;
    }

    /** @return application capabilities */
    public String[] getCapabilities() {
        return mCapabilities == null ? null : Arrays.copyOf(mCapabilities, mCapabilities.length);
    }

    private CastMediaSource(
            String sourceId,
            String applicationId,
            String clientId,
            String autoJoinPolicy,
            String[] capabilities) {
        mSourceId = sourceId;
        mApplicationId = applicationId;
        mClientId = clientId;
        mAutoJoinPolicy = autoJoinPolicy == null ? AUTOJOIN_TAB_AND_ORIGIN_SCOPED : autoJoinPolicy;
        mCapabilities = capabilities;
    }

    @Nullable
    private static String extractParameter(String[] fragments, String key) {
        String keyPrefix = key + "=";
        for (String parameter : fragments) {
            if (parameter.startsWith(keyPrefix)) return parameter.substring(keyPrefix.length());
        }
        return null;
    }

    @Nullable
    private static String[] extractCapabilities(String capabilitiesParameter) {
        if (capabilitiesParameter.length()
                < CAST_APP_CAPABILITIES_PREFIX.length() + CAST_APP_CAPABILITIES_SUFFIX.length()) {
            return null;
        }

        if (!capabilitiesParameter.startsWith(CAST_APP_CAPABILITIES_PREFIX)
                || !capabilitiesParameter.endsWith(CAST_APP_CAPABILITIES_SUFFIX)) {
            return null;
        }

        String capabilitiesList =
                capabilitiesParameter.substring(
                        CAST_APP_CAPABILITIES_PREFIX.length(),
                        capabilitiesParameter.length() - CAST_APP_CAPABILITIES_SUFFIX.length());
        String[] capabilities = capabilitiesList.split(CAST_APP_CAPABILITIES_SEPARATOR);
        for (String capability : capabilities) {
            if (!CAST_APP_CAPABILITIES.contains(capability)) return null;
        }
        return capabilities;
    }

    /**
     * Helper method to create a MediaSource object from a Cast (cast:) presentation URL.
     * @param sourceId the source id for the Cast media source.
     * @return an initialized media source if the uri is a valid Cast presentation URL, null
     * otherwise.
     */
    @Nullable
    private static CastMediaSource fromCastUrl(String sourceId) {
        // Strip the scheme as the Uri parser works better without it.
        Uri sourceUri = Uri.parse(sourceId.substring(CAST_URL_PROTOCOL.length()));
        String applicationId = sourceUri.getPath();
        if (applicationId == null) return null;

        String clientId = sourceUri.getQueryParameter(CAST_URL_CLIENT_ID);
        String autoJoinPolicy = sourceUri.getQueryParameter(CAST_URL_AUTOJOIN_POLICY);
        if (autoJoinPolicy != null && !AUTOJOIN_POLICIES.contains(autoJoinPolicy)) {
            return null;
        }

        String[] capabilities = null;
        String capabilitiesParam = sourceUri.getQueryParameter(CAST_URL_CAPABILITIES);
        if (capabilitiesParam != null) {
            capabilities = capabilitiesParam.split(CAST_APP_CAPABILITIES_SEPARATOR);
            for (String capability : capabilities) {
                if (!CAST_APP_CAPABILITIES.contains(capability)) return null;
            }
        }

        return new CastMediaSource(sourceId, applicationId, clientId, autoJoinPolicy, capabilities);
    }

    /**
     * @deprecated Legacy Cast Presentation URLs are deprecated in favor of cast: URLs.
     *     TODO(crbug.com/40536148): remove this method when we drop support for legacy URLs. Helper
     *     method to create a MediaSource object from a legacy (https:) presentation URL.
     * @param sourceId the source id for the Cast media source.
     * @return an initialized media source if the uri is a valid https presentation URL, null
     *     otherwise.
     */
    @Deprecated
    @Nullable
    private static CastMediaSource fromLegacyUrl(String sourceId) {
        Uri sourceUri = Uri.parse(sourceId);
        String uriFragment = sourceUri.getFragment();
        if (uriFragment == null) return null;

        String[] parameters = uriFragment.split(CAST_SOURCE_ID_SEPARATOR);

        String applicationId = extractParameter(parameters, CAST_SOURCE_ID_APPLICATION_ID);
        if (applicationId == null) return null;

        String[] capabilities = null;
        int capabilitiesIndex = applicationId.indexOf(CAST_APP_CAPABILITIES_PREFIX);
        if (capabilitiesIndex != -1) {
            capabilities = extractCapabilities(applicationId.substring(capabilitiesIndex));
            if (capabilities == null) return null;

            applicationId = applicationId.substring(0, capabilitiesIndex);
        }

        String clientId = extractParameter(parameters, CAST_SOURCE_ID_CLIENT_ID);
        String autoJoinPolicy = extractParameter(parameters, CAST_SOURCE_ID_AUTOJOIN_POLICY);
        if (autoJoinPolicy != null && !AUTOJOIN_POLICIES.contains(autoJoinPolicy)) {
            return null;
        }
        return new CastMediaSource(sourceId, applicationId, clientId, autoJoinPolicy, capabilities);
    }
}
