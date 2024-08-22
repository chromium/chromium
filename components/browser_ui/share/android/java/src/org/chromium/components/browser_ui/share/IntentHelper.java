// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.share;

import android.content.Intent;
import android.net.Uri;
import android.text.Html;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileProviderUtils;

import java.io.File;

/** Helper for issuing intents to the android framework. */
public abstract class IntentHelper {
    private IntentHelper() {}

    /**
     * Triggers a send email intent.  If no application has registered to receive these intents,
     * this will fail silently.
     *  @param email The email address to send to.
     * @param subject The subject of the email.
     * @param body The body of the email.
     * @param chooserTitle The title of the activity chooser.
     * @param fileToAttach The file name of the attachment.
     */
    @SuppressWarnings("deprecation") // Update usage of Html.fromHtml when API min is 24
    @CalledByNative
    static void sendEmail(
            String email, String subject, String body, String chooserTitle, String fileToAttach) {
        Intent send = new Intent(Intent.ACTION_SEND);
        send.setType("message/rfc822");
        if (!TextUtils.isEmpty(email)) send.putExtra(Intent.EXTRA_EMAIL, new String[] {email});
        send.putExtra(Intent.EXTRA_SUBJECT, subject);
        send.putExtra(Intent.EXTRA_TEXT, Html.fromHtml(body));
        if (!TextUtils.isEmpty(fileToAttach)) {
            File fileIn = new File(fileToAttach);
            Uri fileUri;
            // Attempt to use a content Uri, for greater compatibility.  If the path isn't set
            // up to be shared that way with a <paths> meta-data element, just use a file Uri
            // instead.
            try {
                fileUri = FileProviderUtils.getContentUriFromFile(fileIn);
            } catch (IllegalArgumentException ex) {
                fileUri = Uri.fromFile(fileIn);
            }
            send.putExtra(Intent.EXTRA_STREAM, fileUri);
        }

        try {
            Intent chooser = Intent.createChooser(send, chooserTitle);
            // we start this activity outside the main activity.
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ContextUtils.getApplicationContext().startActivity(chooser);
        } catch (android.content.ActivityNotFoundException ex) {
            // If no app handles it, do nothing.
        }
    }
}
