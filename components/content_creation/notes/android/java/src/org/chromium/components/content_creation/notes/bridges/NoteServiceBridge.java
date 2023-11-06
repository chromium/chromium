// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.bridges;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
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

    @NativeMethods
    interface Natives {
        void getTemplates(long nativeNoteServiceBridge, NoteServiceBridge caller,
                Callback<List<NoteTemplate>> callback);
    }
}