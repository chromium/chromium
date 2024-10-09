// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

/**
 * This interface is for consumer to consume the captured content.
 *
 * The consumer shall call OnscreenContentProvider.addConsumer() to get the content and
 * removeConsumer if the content is no longer needed.
 */
public interface ContentCaptureConsumer {
    /**
     * Invoked when the content is captured from a frame.
     * @param parentFrame is the parent of the frame from that the content captured.
     * @param contentCaptureFrame is the captured content tree, its root is the frame.
     */
    void onContentCaptured(FrameSession parentFrame, ContentCaptureFrame contentCaptureFrame);

    /**
     * Invoked when the content is updated in a frame.
     * @param parentFrame is the parent of the frame from that the content captured.
     * @param contentCaptureFrame is the captured content tree, its root is the frame.
     */
    void onContentUpdated(FrameSession parentFrame, ContentCaptureFrame contentCaptureFrame);

    /**
     * Invoked when the session is removed
     * @param session is the removed frame.
     */
    void onSessionRemoved(FrameSession session);

    /**
     * Invoked when the content is removed from a frame
     * @param session defines the frame from that the content removed
     * @param removedIds are array of removed content id.
     */
    void onContentRemoved(FrameSession session, long[] removedIds);

    /**
     * Invoked when the title is updated.
     * @param mainFrame the frame whose title is updated.
     */
    void onTitleUpdated(ContentCaptureFrame mainFrame);

    /**
     * Invoked when the favicon is updated.
     *
     * @param mainFrame the frame whose favicon is updated.
     */
    void onFaviconUpdated(ContentCaptureFrame mainFrame);

    /**
     * @return if the urls shall be captured.
     *     <p>The content of urls might still streamed to the consumer even false is returned. The
     *     consumer shall filter the content upon receiving it.
     */
    boolean shouldCapture(String[] urls);
}
