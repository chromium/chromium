// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.bridges;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.content_creation.notes.NoteService;
import org.chromium.components.content_creation.notes.models.NoteTemplate;

import java.util.List;

/**
 * Bridge class in charge of forwarding requests to the native counterpart of
 * the note service bridge.
 */
@JNINamespace("content_creation")
public class NoteServiceBridge implements NoteService {
    private long mNativeNoteServiceBridge;

    @CalledByNative
    private static NoteServiceBridge create(long nativePtr) {
        return new NoteServiceBridge(nativePtr);
    }

    private NoteServiceBridge(long nativePtr) {
        mNativeNoteServiceBridge = nativePtr;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeNoteServiceBridge = 0;
    }

    @Override
    public void getTemplates(Callback<List<NoteTemplate>> callback) {
        if (mNativeNoteServiceBridge == 0) return;
        NoteServiceBridgeJni.get().getTemplates(mNativeNoteServiceBridge, this, callback);
    }

    @Override
    public boolean isPublishAvailable() {
        if (mNativeNoteServiceBridge == 0) return false;
        return NoteServiceBridgeJni.get().isPublishAvailable(mNativeNoteServiceBridge, this);
    }

    @Override
    public void publishNote(String selectedText, String shareUrl, Callback<String> callback) {
        if (mNativeNoteServiceBridge == 0) return;
        NoteServiceBridgeJni.get().publishNote(
                mNativeNoteServiceBridge, this, selectedText, shareUrl, callback);
    }

    @NativeMethods
    interface Natives {
        void getTemplates(long nativeNoteServiceBridge, NoteServiceBridge caller,
                Callback<List<NoteTemplate>> callback);

        boolean isPublishAvailable(long nativeNoteServiceBridge, NoteServiceBridge caller);

        void publishNote(long nativeNoteServiceBridge, NoteServiceBridge caller,
                String selectedText, String shareUrl, Callback<String> callback);
    }
}