// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;
import android.view.View;
import android.view.View.OnDragListener;
import android.webkit.MimeTypeMap;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.Locale;

/**
 * Handles drag and drop events for the LocationBar. Accepts drops of text, images, or PDFs, and
 * loads them as URLs. Manages DragAndDropPermissions for content URIs.
 */
@NullMarked
public class LocationBarDragDropHandler implements OnDragListener {
    private final OmniboxStub mOmniboxStub;
    private final LocationBarDataProvider mDataProvider;

    /**
     * Creates a new LocationBarDragDropHandler.
     *
     * @param omniboxStub The OmniboxStub used to load URLs.
     * @param dataProvider The LocationBarDataProvider used to get the current tab.
     */
    public LocationBarDragDropHandler(
            OmniboxStub omniboxStub, LocationBarDataProvider dataProvider) {
        mOmniboxStub = omniboxStub;
        mDataProvider = dataProvider;
    }

    @Override
    public boolean onDrag(View v, DragEvent event) {
        switch (event.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                return isAcceptableDrag(event.getClipDescription());
            case DragEvent.ACTION_DROP:
                return acceptDrop(v.getContext(), event);
        }
        return false;
    }

    /**
     * Checks if the drag event contains acceptable content. Accepted content includes text, images,
     * PDFs, or a list of URIs.
     *
     * @param desc The description of the dragged content.
     * @return True if the drag is acceptable, false otherwise.
     */
    @VisibleForTesting
    boolean isAcceptableDrag(ClipDescription desc) {
        if (desc == null) return false;
        if (desc.hasMimeType(ClipDescription.MIMETYPE_TEXT_URILIST)) return true;
        for (int i = 0; i < desc.getMimeTypeCount(); i++) {
            if (isAcceptableMimeType(desc.getMimeType(i))) {
                return true;
            }
        }
        return false;
    }

    /**
     * Handles the file drop event. Attempts to find a suitable URI to load, requests permissions if
     * it's a content URI, and registers an observer to release permissions when loading finishes or
     * fails.
     *
     * @param context The context of the view receiving the drop.
     * @param event The drag event containing the dropped content.
     * @return True if the drop was handled successfully, false otherwise.
     */
    @VisibleForTesting
    boolean acceptDrop(Context context, DragEvent event) {
        ClipData clipData = event.getClipData();
        if (clipData == null) return false;

        Tab tab = mDataProvider.getTab();
        if (tab == null) return false;

        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null) return false;

        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) return false;

        DragAndDropPermissions permissions =
                hasContentUri(clipData) ? activity.requestDragAndDropPermissions(event) : null;

        try {
            Uri uriToLoad = findUriToLoad(context, clipData, event.getClipDescription());
            if (uriToLoad == null) return false;

            String url = uriToLoad.toString();
            if (permissions != null
                    && ContentResolver.SCHEME_CONTENT.equals(uriToLoad.getScheme())) {
                tab.addObserver(new DragAndDropPermissionsReleaseObserver(tab, permissions));
                permissions = null;
            }
            mOmniboxStub.loadUrl(
                    new OmniboxLoadUrlParams.Builder(url, PageTransition.TYPED).build());
            return true;
        } finally {
            if (permissions != null) {
                permissions.release();
            }
        }
    }

    // Mime Utilities

    /**
     * Checks if the given mime type is acceptable for loading. Acceptable types are Text, Image,
     * and PDF. Excludes URI lists.
     *
     * @param mimeType The mime type to check.
     * @return True if acceptable, false otherwise.
     */
    @VisibleForTesting
    boolean isAcceptableMimeType(String mimeType) {
        if (mimeType == null) return false;
        if (ClipDescription.MIMETYPE_TEXT_URILIST.equals(mimeType)) return false;
        return switch (MimeTypeUtils.getTypeFromMimeType(mimeType)) {
            case MimeTypeUtils.Type.TEXT, MimeTypeUtils.Type.IMAGE, MimeTypeUtils.Type.PDF -> true;
            default -> false;
        };
    }

    /**
     * Determines the mime type of a URI. Supports content URIs (via ContentResolver) and file URIs
     * (via extension mapping).
     *
     * @param context The context used to resolve content URIs.
     * @param uri The URI to check.
     * @return The mime type, or null if it cannot be determined.
     */
    @VisibleForTesting
    @Nullable
    String getMimeType(Context context, Uri uri) {
        if (ContentResolver.SCHEME_CONTENT.equals(uri.getScheme())) {
            return context.getContentResolver().getType(uri);
        }
        if (!ContentResolver.SCHEME_FILE.equals(uri.getScheme())) {
            return null;
        }
        String extension = MimeTypeMap.getFileExtensionFromUrl(uri.toString());
        if (extension == null) {
            return null;
        }
        return MimeTypeMap.getSingleton()
                .getMimeTypeFromExtension(extension.toLowerCase(Locale.getDefault()));
    }

    /**
     * Extracts an acceptable fallback mime type from the clip description. Excludes generic URI
     * list mime type.
     *
     * @param desc The clip description to inspect.
     * @return An acceptable mime type, or null if none is found.
     */
    @VisibleForTesting
    @Nullable
    String getFallbackMimeType(ClipDescription desc) {
        for (int i = 0; i < desc.getMimeTypeCount(); i++) {
            String type = desc.getMimeType(i);
            if (isAcceptableMimeType(type)) {
                return type;
            }
        }
        return null;
    }

    // Drop Utilities

    /**
     * Checks if the clip data contains at least one content URI.
     *
     * @param clipData The clip data to check.
     * @return True if a content URI is found, false otherwise.
     */
    @VisibleForTesting
    boolean hasContentUri(ClipData clipData) {
        for (int i = 0; i < clipData.getItemCount(); i++) {
            Uri uri = clipData.getItemAt(i).getUri();
            if (uri != null && ContentResolver.SCHEME_CONTENT.equals(uri.getScheme())) {
                return true;
            }
        }
        return false;
    }

    /**
     * Finds the first acceptable URI in the clip data to load. If the clip data contains a single
     * item and its mime type cannot be determined from the URI, it falls back to checking the clip
     * description.
     *
     * @param context The context used to resolve content URIs.
     * @param clipData The clip data containing the items.
     * @param desc The clip description, used as fallback.
     * @return The first acceptable URI, or null if none is found.
     */
    @VisibleForTesting
    @Nullable
    Uri findUriToLoad(Context context, ClipData clipData, @Nullable ClipDescription desc) {
        for (int i = 0; i < clipData.getItemCount(); i++) {
            ClipData.Item item = clipData.getItemAt(i);
            Uri uri = item.getUri();
            if (uri == null) continue;

            String mimeType = getMimeType(context, uri);
            if (mimeType == null && clipData.getItemCount() == 1 && desc != null) {
                mimeType = getFallbackMimeType(desc);
            }

            if (mimeType != null && isAcceptableMimeType(mimeType)) {
                return uri;
            }
        }
        return null;
    }

    /**
     * A TabObserver that releases DragAndDropPermissions when the page finishes loading, fails to
     * load, or the tab is destroyed. It also removes itself from the tab's observers.
     */
    private static class DragAndDropPermissionsReleaseObserver extends EmptyTabObserver {
        private final Tab mTab;
        private final DragAndDropPermissions mPermissions;

        public DragAndDropPermissionsReleaseObserver(Tab tab, DragAndDropPermissions permissions) {
            mTab = tab;
            mPermissions = permissions;
        }

        @Override
        public void onPageLoadFinished(Tab tab, GURL url) {
            cleanup();
        }

        @Override
        public void onPageLoadFailed(Tab tab, int errorCode) {
            cleanup();
        }

        @Override
        public void onDestroyed(Tab tab) {
            cleanup();
        }

        private void cleanup() {
            mTab.removeObserver(this);
            mPermissions.release();
        }
    }
}
