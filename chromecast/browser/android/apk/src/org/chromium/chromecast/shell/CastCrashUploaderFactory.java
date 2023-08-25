// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;

/**
 * Factory class that creates CastCrashUploader using a ScheduledExecutorService singleton to
 * schedule all tasks on one thread.
 */
public final class CastCrashUploaderFactory {
    private static ScheduledExecutorService sExecutorService;
    public static CastCrashUploader createCastCrashUploader(String crashDumpPath,
            String crashReportsPath, String uuid, String applicationFeedback,
            boolean uploadCrashToStaging) {
        if (sExecutorService == null) {
            sExecutorService = Executors.newScheduledThreadPool(1);
        }
        ElidedLogcatProvider logcatProvider = new AndroidAppLogcatProvider();
        return new CastCrashUploader(sExecutorService, logcatProvider, crashDumpPath,
                crashReportsPath, uuid, applicationFeedback, uploadCrashToStaging);
    }
}
