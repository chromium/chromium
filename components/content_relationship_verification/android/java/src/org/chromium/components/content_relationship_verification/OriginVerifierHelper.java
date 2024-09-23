// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_relationship_verification;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources.NotFoundException;
import android.os.Bundle;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.components.embedder_support.util.Origin;

import java.util.HashSet;
import java.util.Set;

/** Helper functions for the OriginVerification. */
public class OriginVerifierHelper {
    private static final String TAG = "OriginVerifierHelper";

    private static final String ASSET_STATEMENTS_METADATA_KEY = "asset_statements";
    private static final String STATEMENT_RELATION_KEY = "relation";
    private static final String STATEMENT_TARGET_KEY = "target";
    private static final String STATEMENT_NAMESPACE_KEY = "namespace";
    private static final String STATEMENT_SITE_KEY = "site";

    private OriginVerifierHelper() {}

    /** Returns the Digital Asset Links declared in the Android Manifest. */
    public static Set<Origin> getClaimedOriginsFromManifest(String packageName, Context context) {
        try {
            Bundle metaData =
                    context.getPackageManager()
                            .getApplicationInfo(packageName, PackageManager.GET_META_DATA)
                            .metaData;
            int statementsStringId;
            if (metaData == null
                    || (statementsStringId = metaData.getInt(ASSET_STATEMENTS_METADATA_KEY)) == 0) {
                return new HashSet<>();
            }

            // Application context cannot access hosting apps resources.
            String assetStatement = context.getResources().getString(statementsStringId);
            JSONArray statements = new JSONArray(assetStatement);

            if (statements == null) {
                return new HashSet<>();
            }

            Set<Origin> parsedOrigins = new HashSet<>();
            for (int i = 0; i < statements.length(); i++) {
                JSONObject statement = statements.getJSONObject(i);
                // TODO(crbug.com/40243409): Check if lower/upper case is important.

                JSONArray relations = statement.getJSONArray(STATEMENT_RELATION_KEY);
                boolean foundRelation = false;
                for (int j = 0; j < relations.length(); j++) {
                    String relation = relations.getString(j).toString().replace("\\/", "/");
                    if (relation.equals(OriginVerifier.HANDLE_ALL_URLS)) {
                        foundRelation = true;
                        break;
                    }
                }
                if (!foundRelation) continue;

                JSONObject statementTarget = statement.getJSONObject(STATEMENT_TARGET_KEY);
                if (statementTarget.getString(STATEMENT_NAMESPACE_KEY).equals("web")) {
                    parsedOrigins.add(
                            Origin.create(
                                    statementTarget
                                            .getString(STATEMENT_SITE_KEY)
                                            .replace("\\/", "/")));
                }
            }
            return parsedOrigins;
        } catch (PackageManager.NameNotFoundException e) {
            Log.w(
                    TAG,
                    "Failed to read claimed origins from Manifest; "
                            + "PackageManager.NameNotFoundException raised");
        } catch (JSONException e) {
            Log.w(TAG, "Failed to read claimed origins from Manifest, failed to parse JSON");
        } catch (NotFoundException e) {
            Log.w(TAG, "Failed to read claimed origins from Manifest, invalid json content");
        }
        return new HashSet<>();
    }
}
