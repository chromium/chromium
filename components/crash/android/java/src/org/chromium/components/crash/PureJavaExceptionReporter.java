// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.crash;

import android.annotation.SuppressLint;
import android.os.Build;
import android.util.Log;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.PiiElider;
import org.chromium.base.StrictModeContext;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.BuildConfig;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicReferenceArray;

/**
 * Creates a crash report and uploads it to crash server if there is a Java exception.
 *
 * This class is written in pure Java, so it can handle exception happens before native is loaded.
 */
public abstract class PureJavaExceptionReporter
        implements PureJavaExceptionHandler.JavaExceptionReporter {
    // report fields, please keep the name sync with MIME blocks in breakpad_linux.cc
    public static final String CHANNEL = "channel";
    public static final String VERSION = "ver";
    public static final String PRODUCT = "prod";
    public static final String ANDROID_BUILD_ID = "android_build_id";
    public static final String ANDROID_BUILD_FP = "android_build_fp";
    // android-sdk-int and sdk are expected to have the same value.
    // android-sdk-int is needed for compatibility with the C++ crashpad implementation.
    // sdk should be maintained for potential custom monitoring.
    public static final String SDK = "sdk";
    public static final String ANDROID_SDK_INT = "android-sdk-int";
    public static final String DEVICE = "device";
    public static final String GMS_CORE_VERSION = "gms_core_version";
    public static final String INSTALLER_PACKAGE_NAME = "installer_package_name";
    public static final String ABI_NAME = "abi_name";
    public static final String PACKAGE = "package";
    public static final String MODEL = "model";
    public static final String BRAND = "brand";
    public static final String BOARD = "board";
    public static final String EXCEPTION_INFO = "exception_info";
    public static final String PROCESS_TYPE = "ptype";
    public static final String EARLY_JAVA_EXCEPTION = "early_java_exception";
    public static final String CUSTOM_THEMES = "custom_themes";
    public static final String RESOURCES_VERSION = "resources_version";

    private static final String DUMP_LOCATION_SWITCH = "breakpad-dump-location";
    private static final String FILE_SUFFIX = ".dmp";
    private static final String RN = "\r\n";
    private static final String FORM_DATA_MESSAGE = "Content-Disposition: form-data; name=\"";

    private boolean mUpload;
    protected Map<String, String> mReportContent;
    protected File mMinidumpFile;
    private FileOutputStream mMinidumpFileStream;
    private final String mLocalId = UUID.randomUUID().toString().replace("-", "").substring(0, 16);
    private final String mBoundary = "------------" + UUID.randomUUID() + RN;

    private boolean mAttachLogcat;

    public PureJavaExceptionReporter(boolean attachLogcat) {
        mAttachLogcat = attachLogcat;
    }

    @Override
    public void createAndUploadReport(Throwable javaException) {
        // It is OK to do IO in main thread when we know there is a crash happens.
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            createReportContent(javaException);
            createReportFile();
            uploadReport();
        }
    }

    private void addPairedString(String messageType, String messageData) {
        addString(mBoundary);
        addString(FORM_DATA_MESSAGE + messageType + "\"");
        addString(RN + RN + messageData + RN);
    }

    private void addString(String s) {
        try {
            mMinidumpFileStream.write(ApiCompatibilityUtils.getBytesUtf8(s));
        } catch (IOException e) {
            // Nothing we can do here.
        }
    }

    @SuppressLint("WrongConstant")
    private void createReportContent(Throwable javaException) {
        String processName = ContextUtils.getProcessName();
        if (processName == null || !processName.contains(":")) {
            processName = "browser";
        }

        BuildInfo buildInfo = BuildInfo.getInstance();
        mReportContent = new HashMap<>();
        mReportContent.put(PRODUCT, getProductName());
        mReportContent.put(PROCESS_TYPE, processName);
        mReportContent.put(DEVICE, Build.DEVICE);
        mReportContent.put(VERSION, VersionInfo.getProductVersion());
        mReportContent.put(CHANNEL, getChannel());
        mReportContent.put(ANDROID_BUILD_ID, Build.ID);
        mReportContent.put(MODEL, Build.MODEL);
        mReportContent.put(BRAND, Build.BRAND);
        mReportContent.put(BOARD, Build.BOARD);
        mReportContent.put(ANDROID_BUILD_FP, buildInfo.androidBuildFingerprint);
        // ANDROID_SDK_INT and SDK are expected to have the same value.
        // ANDROID_SDK_INT is needed for compatibility with the C++ crashpad implementation.
        // SDK should be maintained for potential custom monitoring.
        mReportContent.put(SDK, String.valueOf(Build.VERSION.SDK_INT));
        mReportContent.put(ANDROID_SDK_INT, String.valueOf(Build.VERSION.SDK_INT));
        mReportContent.put(GMS_CORE_VERSION, buildInfo.getGmsVersionCode());
        mReportContent.put(INSTALLER_PACKAGE_NAME, buildInfo.installerPackageName);
        mReportContent.put(ABI_NAME, buildInfo.abiString);
        mReportContent.put(
                EXCEPTION_INFO,
                PiiElider.sanitizeStacktrace(Log.getStackTraceString(javaException)));
        mReportContent.put(EARLY_JAVA_EXCEPTION, "true");
        mReportContent.put(
                PACKAGE,
                String.format(
                        "%s v%s (%s)",
                        buildInfo.packageName, BuildConfig.VERSION_CODE, buildInfo.versionName));
        mReportContent.put(CUSTOM_THEMES, buildInfo.customThemes);
        mReportContent.put(RESOURCES_VERSION, buildInfo.resourcesVersion);

        AtomicReferenceArray<String> values = CrashKeys.getInstance().getValues();
        for (int i = 0; i < values.length(); i++) {
            String value = values.get(i);
            if (value != null) mReportContent.put(CrashKeys.getKey(i), value);
        }
    }

    protected void createReportFile() {
        try {
            String minidumpFileName = getMinidumpPrefix() + mLocalId + FILE_SUFFIX;
            File minidumpDir = new File(getCrashFilesDirectory(), CrashFileManager.CRASH_DUMP_DIR);
            // Tests disable minidump uploading by not creating the minidump directory.
            mUpload = minidumpDir.exists();
            if (CommandLine.isInitialized()) {
                String overrideMinidumpDirPath =
                        CommandLine.getInstance().getSwitchValue(DUMP_LOCATION_SWITCH);
                if (overrideMinidumpDirPath != null) {
                    minidumpDir = new File(overrideMinidumpDirPath);
                    minidumpDir.mkdirs();
                }
            }
            mMinidumpFile = new File(minidumpDir, minidumpFileName);
            mMinidumpFileStream = new FileOutputStream(mMinidumpFile);
        } catch (FileNotFoundException e) {
            mMinidumpFile = null;
            mMinidumpFileStream = null;
            return;
        }
        for (var e : mReportContent.entrySet()) {
            addPairedString(e.getKey(), e.getValue());
        }
        addString(mBoundary);
        flushToFile();
    }

    private void flushToFile() {
        if (mMinidumpFileStream == null) {
            return;
        }
        try {
            mMinidumpFileStream.flush();
            mMinidumpFileStream.close();
        } catch (Throwable e) {
            mMinidumpFile = null;
        } finally {
            mMinidumpFileStream = null;
        }
    }

    private static String getChannel() {
        if (VersionInfo.isCanaryBuild()) {
            return "canary";
        }
        if (VersionInfo.isDevBuild()) {
            return "dev";
        }
        if (VersionInfo.isBetaBuild()) {
            return "beta";
        }
        if (VersionInfo.isStableBuild()) {
            return "stable";
        }
        return "";
    }

    private void uploadReport() {
        if (mMinidumpFile == null || !mUpload) return;
        if (mAttachLogcat) {
            LogcatCrashExtractor logcatExtractor = new LogcatCrashExtractor();
            mMinidumpFile =
                    logcatExtractor.attachLogcatToMinidump(
                            mMinidumpFile, new CrashFileManager(getCrashFilesDirectory()));
        }
        uploadMinidump(mMinidumpFile);
    }

    /** @return the product name to be used in the crash report. */
    protected abstract String getProductName();

    /**
     * Attempt uploading the given {@code minidump} report immediately.
     *
     * @param minidump the minidump file to be uploaded.
     */
    protected abstract void uploadMinidump(File minidump);

    /** @return prefix to be added before the minidump file name. */
    protected abstract String getMinidumpPrefix();

    /** @return The top level directory where all crash related files are stored. */
    protected abstract File getCrashFilesDirectory();
}
