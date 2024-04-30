// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashMap;

/**
 * This class associates a session id with a web contents in order to allow new activity instances
 * to get the WebContents. This is necessary instead of parceling because other apps cannot use an
 * Intent with a parceled WebContents.
 */
public class WebContentsRegistry {
    private static class WebContentsHolder {
        public WebContents webContents;
        public boolean initialized;

        public WebContentsHolder(WebContents webContents) {
            this.webContents = webContents;
            this.initialized = false;
        }
    }

    private static final HashMap<String, WebContentsHolder> sSesionIdToWebContents =
            new HashMap<>();

    static void addWebContents(String sessionId, WebContents contents) {
        sSesionIdToWebContents.put(sessionId, new WebContentsHolder(contents));
    }

    static void removeWebContents(String sessionId) {
        sSesionIdToWebContents.remove(sessionId);
    }

    public static WebContents getWebContents(String sessionId) {
        WebContentsHolder result = sSesionIdToWebContents.get(sessionId);
        return result == null ? null : result.webContents;
    }

    public static void initializeWebContents(
            WebContents webContents, ContentView contentView, WindowAndroid window) {
        for (WebContentsHolder holder : sSesionIdToWebContents.values()) {
            if (holder.webContents.equals(webContents)) {
                if (!holder.initialized) {
                    // TODO(derekjchow): productVersion
                    webContents.setDelegates(
                            "",
                            ViewAndroidDelegate.createBasicDelegate(contentView),
                            contentView,
                            window,
                            WebContents.createDefaultInternalsHolder());
                } else {
                    webContents.getViewAndroidDelegate().setContainerView(contentView);
                    webContents.setTopLevelNativeWindow(window);
                }
                holder.initialized = true;
                return;
            }
        }
    }
}
