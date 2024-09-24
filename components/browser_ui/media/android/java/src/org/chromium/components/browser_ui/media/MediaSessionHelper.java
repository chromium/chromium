// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.SysUtils;
import org.chromium.components.browser_ui.media.MediaSessionUma.MediaSessionActionSource;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.MediaSessionObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.media_session.mojom.MediaSessionAction;
import org.chromium.services.media_session.MediaImage;
import org.chromium.services.media_session.MediaMetadata;
import org.chromium.services.media_session.MediaPosition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * Glue code that relays events from the {@link org.chromium.content.browser.MediaSession} for a
 * WebContents to a delegate (ultimately, to {@link MediaNotificationController}).
 */
public class MediaSessionHelper implements MediaImageCallback {
    private static final String UNICODE_PLAY_CHARACTER = "\u25B6";
    @VisibleForTesting public static final int HIDE_NOTIFICATION_DELAY_MILLIS = 2500;

    private Delegate mDelegate;
    private WebContents mWebContents;
    @VisibleForTesting public WebContentsObserver mWebContentsObserver;
    @VisibleForTesting public MediaSessionObserver mMediaSessionObserver;
    private MediaImageManager mMediaImageManager;
    private Bitmap mPageMediaImage;
    @VisibleForTesting public Bitmap mFavicon;
    private Bitmap mCurrentMediaImage;
    private String mOrigin;
    private int mPreviousVolumeControlStream = AudioManager.USE_DEFAULT_STREAM_TYPE;
    @VisibleForTesting public MediaNotificationInfo.Builder mNotificationInfoBuilder;
    // The fallback title if |mPageMetadata| is null or its title is empty.
    private String mFallbackTitle;
    // Set to true if favicon update callback was called at least once.
    private boolean mMaybeHasFavicon;
    // The metadata set by the page.
    private MediaMetadata mPageMetadata;
    // The currently showing metadata.
    private MediaMetadata mCurrentMetadata;
    private Set<Integer> mMediaSessionActions = Collections.emptySet();
    private @Nullable MediaPosition mMediaPosition;
    private Handler mHandler;
    // The delayed task to hide notification. Hiding notification can be immediate or delayed.
    // Delayed hiding will schedule this delayed task to |mHandler|. The task will be canceled when
    // showing or immediate hiding.
    private Runnable mHideNotificationDelayedTask;
    @VisibleForTesting public LargeIconBridge mLargeIconBridge;

    // Used to override the MediaSession object get from WebContents. This is to work around the
    // static getter {@link MediaSession#fromWebContents()}.
    @VisibleForTesting public static MediaSession sOverriddenMediaSession;

    private MediaNotificationListener mControlsListener =
            new MediaNotificationListener() {
                @Override
                public void onPlay(int actionSource) {
                    if (isNotificationHidingOrHidden()) return;

                    MediaSessionUma.recordPlay(
                            MediaSessionHelper.convertMediaActionSourceToUMA(actionSource));

                    if (mMediaSessionObserver.getMediaSession() == null) return;

                    mMediaSessionObserver.getMediaSession().resume();
                }

                @Override
                public void onPause(int actionSource) {
                    if (isNotificationHidingOrHidden()) return;

                    if (mMediaSessionObserver.getMediaSession() == null) return;

                    mMediaSessionObserver.getMediaSession().suspend();
                }

                @Override
                public void onStop(int actionSource) {
                    if (isNotificationHidingOrHidden()) return;

                    if (mMediaSessionObserver.getMediaSession() != null) {
                        mMediaSessionObserver.getMediaSession().stop();
                    }
                }

                @Override
                public void onMediaSessionAction(int action) {
                    if (!MediaSessionAction.isKnownValue(action)) return;
                    if (mMediaSessionObserver != null) {
                        mMediaSessionObserver.getMediaSession().didReceiveAction(action);
                    }
                }

                @Override
                public void onMediaSessionSeekTo(long pos) {
                    if (mMediaSessionObserver == null) return;
                    mMediaSessionObserver.getMediaSession().seekTo(pos);
                }
            };

    private void hideNotificationDelayed() {
        if (mWebContentsObserver == null) return;
        if (mHideNotificationDelayedTask != null) return;

        mHideNotificationDelayedTask =
                new Runnable() {
                    @Override
                    public void run() {
                        mHideNotificationDelayedTask = null;
                        hideNotificationInternal();
                    }
                };
        mHandler.postDelayed(mHideNotificationDelayedTask, HIDE_NOTIFICATION_DELAY_MILLIS);

        mNotificationInfoBuilder = null;
        mFavicon = null;
    }

    private void hideNotificationImmediately() {
        if (mWebContentsObserver == null) return;
        if (mHideNotificationDelayedTask != null) {
            mHandler.removeCallbacks(mHideNotificationDelayedTask);
            mHideNotificationDelayedTask = null;
        }

        hideNotificationInternal();
        mNotificationInfoBuilder = null;
    }

    /**
     * This method performs the common steps for hiding the notification. It should only be called
     * by {@link #hideNotificationDelayed()} and {@link #hideNotificationImmediately()}.
     */
    private void hideNotificationInternal() {
        mDelegate.hideMediaNotification();
        Activity activity = getActivity();
        if (activity != null) {
            activity.setVolumeControlStream(mPreviousVolumeControlStream);
        }
    }

    private void showNotification() {
        assert mNotificationInfoBuilder != null;
        if (mHideNotificationDelayedTask != null) {
            mHandler.removeCallbacks(mHideNotificationDelayedTask);
            mHideNotificationDelayedTask = null;
        }
        mDelegate.showMediaNotification(mNotificationInfoBuilder.build());
    }

    private MediaSessionObserver createMediaSessionObserver(MediaSession mediaSession) {
        return new MediaSessionObserver(mediaSession) {
            @Override
            public void mediaSessionDestroyed() {
                hideNotificationImmediately();
                cleanupMediaSessionObserver();
            }

            @Override
            public void mediaSessionStateChanged(boolean isControllable, boolean isPaused) {
                if (!isControllable) {
                    hideNotificationDelayed();
                    return;
                }

                Intent contentIntent = mDelegate.createBringTabToFrontIntent();

                if (mFallbackTitle == null) mFallbackTitle = sanitizeMediaTitle(mOrigin);

                mCurrentMetadata = getMetadata();
                mCurrentMediaImage = getCachedNotificationImage();
                rebaseMediaPosition(isPaused);
                mNotificationInfoBuilder =
                        mDelegate
                                .createMediaNotificationInfoBuilder()
                                .setMetadata(mCurrentMetadata)
                                .setPaused(isPaused)
                                .setOrigin(mOrigin)
                                .setPrivate(mWebContents.isIncognito())
                                .setNotificationSmallIcon(R.drawable.audio_playing)
                                .setNotificationLargeIcon(mCurrentMediaImage)
                                .setMediaSessionImage(mPageMediaImage)
                                .setActions(
                                        MediaNotificationInfo.ACTION_PLAY_PAUSE
                                                | MediaNotificationInfo.ACTION_SWIPEAWAY
                                                | MediaNotificationInfo.ACTION_STOP)
                                .setContentIntent(contentIntent)
                                .setListener(mControlsListener)
                                .setMediaSessionActions(mMediaSessionActions)
                                .setMediaPosition(mMediaPosition);

                // Show a default icon in incognito contents, as they don't show the media icon.
                // Also show a default icon if we won't get a favicon from {@link mDelegate}. If the
                // delegate will pass a favicon later, show nothing for now; we expect the favicon
                // to arrive quickly.
                if (mWebContents.isIncognito()
                        || (mCurrentMediaImage == null && !fetchLargeFaviconImage())) {
                    mNotificationInfoBuilder.setDefaultNotificationLargeIcon(
                            R.drawable.audio_playing_square);
                }
                showNotification();
                Activity activity = getActivity();
                if (activity != null) {
                    activity.setVolumeControlStream(AudioManager.STREAM_MUSIC);
                }
            }

            @Override
            public void mediaSessionMetadataChanged(MediaMetadata metadata) {
                mPageMetadata = metadata;
                updateNotificationMetadata();
            }

            @Override
            public void mediaSessionActionsChanged(Set<Integer> actions) {
                mMediaSessionActions = actions;
                updateNotificationActions();
            }

            @Override
            public void mediaSessionArtworkChanged(List<MediaImage> images) {
                mMediaImageManager.downloadImage(images, MediaSessionHelper.this);
                updateNotificationMetadata();
            }

            @Override
            public void mediaSessionPositionChanged(@Nullable MediaPosition position) {
                mMediaPosition = position;
                updateNotificationPosition();
            }

            /**
             * Adjust `mMediaPosition` so that it's unambiguous about what the current media time
             * is.  Otherwise, when transitioning into the paused state, the platform won't know to
             * adjust the time from the `getLastUpdatedTime()`.  This is especially bad since the
             * playback rate in the MediaPosition and `isPaused` don't always agree immediately;
             * we can find out about `isPaused` before being told of the final, playback rate = 0,
             * MediaPosition.  To avoid this, we adjust the MediaPosition based on its current
             * playback rate, and update the playback rate to zero so that it's unambiguous.
             */
            private void rebaseMediaPosition(boolean isPaused) {
                if (mMediaPosition == null) return;

                long now = SystemClock.elapsedRealtime();
                long rebased_position =
                        mMediaPosition.getPosition()
                                + (long)
                                        ((now - mMediaPosition.getLastUpdatedTime())
                                                * mMediaPosition.getPlaybackRate());
                mMediaPosition =
                        new MediaPosition(
                                mMediaPosition.getDuration(),
                                rebased_position,
                                isPaused ? 0 : mMediaPosition.getPlaybackRate(),
                                now);
            }
        };
    }

    public void setWebContents(@Nullable WebContents webContents) {
        if (mWebContents == webContents) return;

        mWebContents = webContents;

        if (mWebContentsObserver != null) mWebContentsObserver.destroy();

        mMediaImageManager.setWebContents(mWebContents);

        if (webContents == null) {
            mWebContentsObserver = null;
            cleanupMediaSessionObserver();
            return;
        }

        mWebContentsObserver =
                new WebContentsObserver(webContents) {
                    @Override
                    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                        if (!navigation.hasCommitted() || navigation.isSameDocument()) {
                            return;
                        }

                        mOrigin =
                                UrlFormatter.formatUrlForDisplayOmitSchemeOmitTrivialSubdomains(
                                        webContents.getVisibleUrl().getOrigin().getSpec());
                        mFavicon = null;
                        mPageMediaImage = null;
                        mPageMetadata = null;
                        // |mCurrentMetadata| selects either |mPageMetadata| or |mFallbackTitle|. As
                        // there is no guarantee {@link #titleWasSet()} will be called before or
                        // after this method, |mFallbackTitle| is not reset in this callback, i.e.
                        // relying solely on {@link #titleWasSet()}. The following assignment is
                        // to keep |mCurrentMetadata| up to date as |mPageMetadata| may have
                        // changed.
                        mCurrentMetadata = getMetadata();
                        mMediaSessionActions = Collections.emptySet();

                        if (isNotificationHidingOrHidden()) return;

                        mNotificationInfoBuilder.setOrigin(mOrigin);
                        mNotificationInfoBuilder.setNotificationLargeIcon(mFavicon);
                        mNotificationInfoBuilder.setMediaSessionImage(mPageMediaImage);
                        mNotificationInfoBuilder.setMetadata(mCurrentMetadata);
                        mNotificationInfoBuilder.setMediaSessionActions(mMediaSessionActions);
                        showNotification();
                    }

                    @Override
                    public void titleWasSet(String title) {
                        String newFallbackTitle = sanitizeMediaTitle(title);
                        if (!TextUtils.equals(mFallbackTitle, newFallbackTitle)) {
                            mFallbackTitle = newFallbackTitle;
                            updateNotificationMetadata();
                        }
                    }

                    @Override
                    public void onVisibilityChanged(@Visibility int visibility) {
                        if (visibility == Visibility.VISIBLE) {
                            mDelegate.activateAndroidMediaSession();
                        }
                    }

                    @Override
                    public void mediaSessionCreated(MediaSession mediaSession) {
                        setUpMediaSessionObserver(mediaSession);
                    }
                };

        MediaSession mediaSession = getMediaSession(webContents);
        setUpMediaSessionObserver(mediaSession);
    }

    private void setUpMediaSessionObserver(MediaSession mediaSession) {
        if (mMediaSessionObserver != null
                && mediaSession == mMediaSessionObserver.getMediaSession()) {
            return;
        }

        cleanupMediaSessionObserver();
        if (mediaSession != null) {
            mMediaSessionObserver = createMediaSessionObserver(mediaSession);
        }
    }

    private void cleanupMediaSessionObserver() {
        if (mMediaSessionObserver == null) return;
        mMediaSessionObserver.stopObserving();
        mMediaSessionObserver = null;
        mMediaSessionActions = Collections.emptySet();
    }

    /** An interface for dispatching embedder-specific behavior. */
    public interface Delegate {
        /** Returns an intent that brings the associated web contents to the front. */
        Intent createBringTabToFrontIntent();

        /** Returns the {@link LargeIconBridge} to be used while obtaining icons. */
        LargeIconBridge getLargeIconBridge();

        /**
         * Creates a {@link MediaNotificationInfo.Builder} with basic embedder-specific
         * initialization.
         */
        public MediaNotificationInfo.Builder createMediaNotificationInfoBuilder();

        /** Shows a notification with the given metadata. */
        void showMediaNotification(MediaNotificationInfo notificationInfo);

        /** Hides the active notification. */
        void hideMediaNotification();

        /** Activates the Android MediaSession. */
        void activateAndroidMediaSession();
    }

    public MediaSessionHelper(@NonNull WebContents webContents, @NonNull Delegate delegate) {
        mDelegate = delegate;
        mMediaImageManager =
                new MediaImageManager(
                        MediaNotificationImageUtils.MINIMAL_MEDIA_IMAGE_SIZE_PX,
                        MediaNotificationImageUtils.getIdealMediaImageSize());
        mHandler = new Handler();
        setWebContents(webContents);

        Activity activity = getActivity();
        if (activity != null) {
            mPreviousVolumeControlStream = activity.getVolumeControlStream();
        }
    }

    /**
     * Called when this object should no longer manage a media session because owning code no longer
     * requires it.
     */
    public void destroy() {
        cleanupMediaSessionObserver();
        hideNotificationImmediately();
        if (mWebContentsObserver != null) mWebContentsObserver.destroy();
        mWebContentsObserver = null;
        if (mLargeIconBridge != null) mLargeIconBridge.destroy();
        mLargeIconBridge = null;
    }

    /**
     * Removes all the leading/trailing white spaces and the quite common unicode play character.
     * It improves the visibility of the title in the notification.
     *
     * @param title The original tab title, e.g. "   ▶   Foo - Bar  "
     * @return The sanitized tab title, e.g. "Foo - Bar"
     */
    private String sanitizeMediaTitle(String title) {
        title = title.trim();
        return title.startsWith(UNICODE_PLAY_CHARACTER) ? title.substring(1).trim() : title;
    }

    /**
     * Converts the {@link MediaNotificationListener} action source enum into the
     * {@link MediaSessionUma} one to ensure matching the histogram values.
     * @param source the source id, must be one of the ACTION_SOURCE_* constants defined in the
     *               {@link MediaNotificationListener} interface.
     * @return the corresponding histogram value.
     */
    public static @Nullable @MediaSessionActionSource Integer convertMediaActionSourceToUMA(
            int source) {
        if (source == MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION) {
            return MediaSessionActionSource.MEDIA_NOTIFICATION;
        } else if (source == MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION) {
            return MediaSessionActionSource.MEDIA_SESSION;
        } else if (source == MediaNotificationListener.ACTION_SOURCE_HEADSET_UNPLUG) {
            return MediaSessionActionSource.HEADSET_UNPLUG;
        }

        assert false;
        return null;
    }

    private Activity getActivity() {
        if (mWebContents == null) return null;

        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return null;

        return windowAndroid.getActivity().get();
    }

    /** Returns true if a large favicon might be found. */
    private boolean fetchLargeFaviconImage() {
        // The page does not have a favicon yet to fetch since onFaviconUpdated was never called.
        // Don't waste time trying to find it.
        if (!mMaybeHasFavicon) return false;

        GURL pageUrl = mWebContents.getLastCommittedUrl();
        int size = MediaNotificationImageUtils.MINIMAL_MEDIA_IMAGE_SIZE_PX;
        if (mLargeIconBridge == null) {
            mLargeIconBridge = mDelegate.getLargeIconBridge();
        }
        LargeIconBridge.LargeIconCallback callback =
                new LargeIconBridge.LargeIconCallback() {
                    @Override
                    public void onLargeIconAvailable(
                            Bitmap icon,
                            int fallbackColor,
                            boolean isFallbackColorDefault,
                            int iconType) {
                        setLargeIcon(icon);
                    }
                };

        return mLargeIconBridge.getLargeIconForUrl(pageUrl, size, callback);
    }

    /**
     * Updates the best favicon if the given icon is better and the favicon is shown in
     * notification.
     */
    public void updateFavicon(Bitmap icon) {
        if (icon == null) return;

        mMaybeHasFavicon = true;

        // Store the favicon only if notification is being shown. Otherwise the favicon is
        // obtained from large icon bridge when needed.
        if (isNotificationHidingOrHidden() || mPageMediaImage != null) return;

        // Disable favicons in notifications for low memory devices on O
        // where the notification icon is optional.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && SysUtils.isLowEndDevice()) return;

        if (!MediaNotificationImageUtils.isBitmapSuitableAsMediaImage(icon)) return;
        if (mFavicon != null
                && (icon.getWidth() < mFavicon.getWidth()
                        || icon.getHeight() < mFavicon.getHeight())) {
            return;
        }
        mFavicon = MediaNotificationImageUtils.downscaleIconToIdealSize(icon);
        updateNotificationImage(mFavicon);
    }

    /** Sets an icon which will preferentially be used in place of a smaller favicon. */
    public void setLargeIcon(Bitmap icon) {
        if (isNotificationHidingOrHidden()) return;

        if (icon == null) {
            // If we do not have any favicon then make sure we show default sound icon. This
            // icon is used by notification manager only if we do not show any icon.
            mNotificationInfoBuilder.setDefaultNotificationLargeIcon(
                    R.drawable.audio_playing_square);
            showNotification();
        } else {
            updateFavicon(icon);
        }
    }

    /**
     * Updates the metadata in media notification. This method should be called whenever
     * |mPageMetadata| or |mFallbackTitle| is changed.
     */
    private void updateNotificationMetadata() {
        if (isNotificationHidingOrHidden()) return;

        MediaMetadata newMetadata = getMetadata();
        if (mCurrentMetadata.equals(newMetadata)) return;

        mCurrentMetadata = newMetadata;
        mNotificationInfoBuilder.setMetadata(mCurrentMetadata);
        showNotification();
    }

    /**
     * @return The up-to-date MediaSession metadata. Returns the cached object like |mPageMetadata|
     * or |mCurrentMetadata| if it reflects the current state. Otherwise will return a new
     * {@link MediaMetadata} object.
     */
    private MediaMetadata getMetadata() {
        String title = mFallbackTitle;
        String artist = "";
        String album = "";
        if (mPageMetadata != null) {
            if (!TextUtils.isEmpty(mPageMetadata.getTitle())) return mPageMetadata;

            artist = mPageMetadata.getArtist();
            album = mPageMetadata.getAlbum();
        }

        if (mCurrentMetadata != null
                && TextUtils.equals(title, mCurrentMetadata.getTitle())
                && TextUtils.equals(artist, mCurrentMetadata.getArtist())
                && TextUtils.equals(album, mCurrentMetadata.getAlbum())) {
            return mCurrentMetadata;
        }

        return new MediaMetadata(title, artist, album);
    }

    private void updateNotificationActions() {
        if (isNotificationHidingOrHidden()) return;

        mNotificationInfoBuilder.setMediaSessionActions(mMediaSessionActions);
        showNotification();
    }

    private void updateNotificationPosition() {
        if (isNotificationHidingOrHidden()) return;

        mNotificationInfoBuilder.setMediaPosition(mMediaPosition);
        showNotification();
    }

    @Override
    public void onImageDownloaded(Bitmap image) {
        mPageMediaImage = MediaNotificationImageUtils.downscaleIconToIdealSize(image);
        mFavicon = null;
        updateNotificationImage(mPageMediaImage);
    }

    private void updateNotificationImage(Bitmap newMediaImage) {
        if (mCurrentMediaImage == newMediaImage) return;

        mCurrentMediaImage = newMediaImage;

        if (isNotificationHidingOrHidden()) return;
        mNotificationInfoBuilder.setNotificationLargeIcon(mCurrentMediaImage);
        mNotificationInfoBuilder.setMediaSessionImage(mPageMediaImage);
        showNotification();
    }

    private Bitmap getCachedNotificationImage() {
        if (mPageMediaImage != null) return mPageMediaImage;
        if (mFavicon != null) return mFavicon;
        return null;
    }

    private boolean isNotificationHidingOrHidden() {
        return mNotificationInfoBuilder == null;
    }

    private MediaSession getMediaSession(WebContents contents) {
        return (sOverriddenMediaSession != null)
                ? sOverriddenMediaSession
                : MediaSession.fromWebContents(contents);
    }
}
