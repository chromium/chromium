// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Automatically extracts context information and serializes it in JSON form.
 */
class FeedbackContext extends JSONObject {
    public static String buildContextString(
            Activity activity, String debugContext, int indentSpaces) {
        try {
            return new FeedbackContext(activity, debugContext).toString(indentSpaces);
        } catch (JSONException e) {
            // Note: it is potentially unsafe to return e.getMessage(): the exception message
            // could be wrangled and used as an attack vector when arriving at the JSON parser.
            return "{\"error\": \"Failed to convert feedback context to string.\"}";
        }
    }

    private FeedbackContext(Activity activity, String debugContext) throws JSONException {
        addActivityInformation(activity);
        addClientContext(debugContext);
    }

    private void addActivityInformation(Activity activity) throws JSONException {
        put("intent-action", activity.getIntent().getAction());
        put("intent-data", activity.getIntent().getDataString());
    }

    private void addClientContext(String debugContext) throws JSONException {
        // Try to parse the debug context as JSON object. If that fails, just add the string as-is.
        try {
            put("debug-context", new JSONObject(debugContext));
        } catch (JSONException encodingException) {
            put("debug-context", debugContext);
        }
    }
}
