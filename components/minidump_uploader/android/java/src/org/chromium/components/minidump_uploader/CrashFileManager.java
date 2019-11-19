// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.Scanner;
import java.util.UUID;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * The CrashFileManager is responsible for managing the "Crash Reports" directory containing
 * minidump files and shepherding them through a state machine represented by the file names.
 *  1. Minidumps are read from Crashpad's CrashReportDatabase and re-written as MIME files in the
 *     "Crash Reports" directory as foo.dmpNNNNN where NNNNN is the PID (process id) of the
 *     crashing process.
 *  2. foo.dmpNNNNN.try0 is a minidump file with recent logcat output attached to it; or a file for
 *     which logcat output has been intentionally omitted. Notably, Webview-generated minidumps do
 *     not include logcat output.
 *  3. foo.dmpNNNNN.tryM for M > 0 is a minidump file that's been attempted to be uploaded to the
 *     crash server, but for which M upload attempts have failed.
 *  4. foo.upNNNNN.tryM names a successfully uploaded file.
 *  5. foo.skippedNNNNN.tryM names for a file whose upload was skipped. An upload may be skipped,
 *     for example, if the user has not consented to uploading crash reports. These files are marked
 *     as skipped rather than deleted immediately to allow the user to manually initiate an upload.
 *  6. foo.forcedNNNNN.tryM names a file that the user has manually requested to upload.
 *  7. foo.tmp is a temporary file.
 */
public class CrashFileManager {
    private static final String TAG = "CrashFileManager";

    /**
     * The name of the crash directory.
     */
    public static final String CRASH_DUMP_DIR = "Crash Reports";

    private static final String CRASHPAD_DIR = "Crashpad";

    // This should mirror the C++ CrashUploadList::kReporterLogFilename variable.
    @VisibleForTesting
    public static final String CRASH_DUMP_LOGFILE = "uploads.log";

    // Local ID is the segment after the last hyphen and before the extensions part. It's usually an
    // alphanumeric value but there is not restriction of having other characters like `_`. So we
    // define the id to be a sequence of non separator characters {`-`, `,` or `.`}
    private static final Pattern CRASH_LOCAL_ID_PATTERN = Pattern.compile("^[^.]+-([^-,]+?)\\.");

    // Unlike the MINIDUMP_ALL_READY_FOR_UPLOAD_PATTERN below, this pattern omits a ".tryN" suffix.
    private static final Pattern MINIDUMP_SANS_LOGCAT_PATTERN =
            Pattern.compile("\\.dmp([0-9]*)\\z");

    // Minidumps that are ready for uploading including forced uploads.
    private static final Pattern MINIDUMP_ALL_READY_FOR_UPLOAD_PATTERN =
            Pattern.compile("\\.(dmp|forced)([0-9]*)(\\.try([0-9]+))\\z");

    // Minidumps that are ready for uploading excluding forced uploads.
    private static final Pattern MINIDUMP_READY_FOR_UPLOAD_PATTERN =
            Pattern.compile("\\.(dmp)([0-9]*)(\\.try([0-9]+))\\z");

    private static final Pattern UPLOADED_MINIDUMP_PATTERN =
            Pattern.compile("\\.up([0-9]*)(\\.try([0-9]+))\\z");

    private static final Pattern MINIDUMP_FORCED_UPLOAD_PATTERN =
            Pattern.compile("\\.forced([0-9]*)(\\.try([0-9]+))\\z");

    private static final Pattern MINIDUMP_SKIPPED_UPLOAD_PATTERN =
            Pattern.compile("\\.skipped([0-9]*)(\\.try([0-9]+))\\z");

    private static final String NOT_YET_UPLOADED_MINIDUMP_SUFFIX = ".dmp";

    private static final String UPLOADED_MINIDUMP_SUFFIX = ".up";

    private static final String UPLOAD_SKIPPED_MINIDUMP_SUFFIX = ".skipped";

    private static final String UPLOAD_FORCED_MINIDUMP_SUFFIX = ".forced";

    private static final String UPLOAD_ATTEMPT_DELIMITER = ".try";

    // The suffix used when a minidump is first ready for upload: ".try0".
    public static final String READY_FOR_UPLOAD_SUFFIX = UPLOAD_ATTEMPT_DELIMITER + "0";

    // A delimiter between uid and the rest of a minidump filename. Only used for WebView minidumps.
    private static final String UID_DELIMITER = "_";

    @VisibleForTesting
    protected static final String TMP_SUFFIX = ".tmp";

    private static final Pattern TMP_PATTERN = Pattern.compile("\\.tmp\\z");

    // The maximum number of non-uploaded crashes that may be kept in the crash reports directory.
    // Chosen to attempt to balance between keeping a generous number of crashes, and not using up
    // too much filesystem storage space for obsolete crash reports.
    @VisibleForTesting
    protected static final int MAX_CRASH_REPORTS_TO_KEEP = 10;

    // The maximum age, in days, considered acceptable for a crash report. Reports older than this
    // age will be removed. The constant is chosen to be quite conservative, while still allowing
    // users to eventually reclaim filesystem storage space from obsolete crash reports.
    private static final int MAX_CRASH_REPORT_AGE_IN_DAYS = 30;

    // The maximum number of non-uploaded crashes to copy to the crash reports directory. The
    // difference between this value and MAX_CRASH_REPORTS_TO_KEEP is that TO_KEEP is only checked
    // when we clean out the crash directory - the TO_UPLOAD value is checked every time we try to
    // copy a minidump - to ensure we don't store too many minidumps before they are cleaned out
    // after being uploaded.
    @VisibleForTesting
    static final int MAX_CRASH_REPORTS_TO_UPLOAD = MAX_CRASH_REPORTS_TO_KEEP * 2;
    // Same as above except this value is enforced per UID, so that one single app can't hog all
    // storage/uploading resources.
    @VisibleForTesting
    static final int MAX_CRASH_REPORTS_TO_UPLOAD_PER_UID = MAX_CRASH_REPORTS_TO_KEEP;

    /**
     * Comparator used for sorting files by modification date.
     * @return Comparator for prioritizing the more recently modified file
     */
    @VisibleForTesting
    protected static final Comparator<File> sFileComparator = new Comparator<File>() {
        @Override
        public int compare(File lhs, File rhs) {
            if (lhs.lastModified() == rhs.lastModified()) {
                return lhs.compareTo(rhs);
            } else if (lhs.lastModified() < rhs.lastModified()) {
                return 1;
            } else {
                return -1;
            }
        }
    };

    /**
     * Delete the file {@param fileToDelete}.
     */
    public static boolean deleteFile(File fileToDelete) {
        boolean isSuccess = fileToDelete.delete();
        if (!isSuccess) {
            Log.w(TAG, "Unable to delete " + fileToDelete.getAbsolutePath());
        }
        return isSuccess;
    }

    /**
     * Returns whether a minidump file definitely lacks logcat output. Note: This method does not
     * provide an "if and only if" test: it may return false for a path that lacks logcat output, if
     * logcat output has been intentionally skipped for that minidump. However, a return value of
     * true means that the file definitely lacks logcat output.
     * @param path The minidump pathname to test.
     * @return Whether the given path corresponds to a minidump file that definitely lacks logcat
     *    output.
     */
    public static boolean isMinidumpSansLogcat(String path) {
        return MINIDUMP_SANS_LOGCAT_PATTERN.matcher(path).find();
    }

    public static String tryIncrementAttemptNumber(File mFileToUpload) {
        String newName = filenameWithIncrementedAttemptNumber(mFileToUpload.getPath());
        return mFileToUpload.renameTo(new File(newName)) ? newName : null;
    }

    /**
     * @return The file name to rename to after an addition attempt to upload
     */
    @VisibleForTesting
    public static String filenameWithIncrementedAttemptNumber(String filename) {
        int numTried = readAttemptNumberInternal(filename);
        if (numTried >= 0) {
            int newCount = numTried + 1;
            return filename.replace(
                    UPLOAD_ATTEMPT_DELIMITER + numTried, UPLOAD_ATTEMPT_DELIMITER + newCount);
        } else {
            // readAttemptNumberInternal returning -1 means there is no UPLOAD_ATTEMPT_DELIMITER in
            // the file name (or that there is a delimiter but no attempt number). So, we have to
            // add the delimiter and attempt number ourselves.
            return filename + UPLOAD_ATTEMPT_DELIMITER + "1";
        }
    }

    /**
     * Attempts to rename the given file to mark it as ready for upload. This should be done when
     * logcat extraction fails or is otherwise intentionally skipped. An equivalent operation is
     * done when extraction succeeds; but since the logcat output needs to be included in the
     * uploaded data, more than a simple rename is needed.
     *
     * @return The renamed file, or null if renaming failed.
     */
    public static File trySetReadyForUpload(File fileToUpload) {
        assert CrashFileManager.isMinidumpSansLogcat(fileToUpload.getName());
        File renamedFile = new File(fileToUpload.getPath() + READY_FOR_UPLOAD_SUFFIX);
        return fileToUpload.renameTo(renamedFile) ? renamedFile : null;
    }

    /**
     * Attempts to rename the given file to mark it as a forced upload. This is useful for allowing
     * users to manually initiate previously skipped uploads.
     *
     * @return The renamed file, or null if renaming failed.
     */
    public static File trySetForcedUpload(File fileToUpload) {
        if (fileToUpload.getName().contains(UPLOADED_MINIDUMP_SUFFIX)) {
            Log.w(TAG, "Refusing to reset upload attempt state for a file that has already been "
                            + "successfully uploaded: " + fileToUpload.getName());
            return null;
        }
        File renamedFile = new File(filenameWithForcedUploadState(fileToUpload.getPath()));
        return fileToUpload.renameTo(renamedFile) ? renamedFile : null;
    }

    /**
     * @return True iff the provided File was manually forced (by the user) to be uploaded.
     */
    public static boolean isForcedUpload(File fileToUpload) {
        return fileToUpload.getName().contains(UPLOAD_FORCED_MINIDUMP_SUFFIX);
    }

    /**
     * @return The filename to rename to so as to manually force an upload (including clearing any
     *     previous upload attempt history).
     */
    @VisibleForTesting
    protected static String filenameWithForcedUploadState(String filename) {
        int numTried = readAttemptNumber(filename);
        if (numTried > 0) {
            filename = filename.replace(
                    UPLOAD_ATTEMPT_DELIMITER + numTried, UPLOAD_ATTEMPT_DELIMITER + 0);
        }
        filename = filename.replace(UPLOAD_SKIPPED_MINIDUMP_SUFFIX, UPLOAD_FORCED_MINIDUMP_SUFFIX);
        return filename.replace(NOT_YET_UPLOADED_MINIDUMP_SUFFIX, UPLOAD_FORCED_MINIDUMP_SUFFIX);
    }

    /**
     * Returns how many times we've tried to upload a certain minidump file.
     * @return The number of attempts to upload the given minidump file, parsed from its filename.
     *     Returns 0 if an attempt number cannot be parsed from the filename.
     */
    public static int readAttemptNumber(String filename) {
        int numTries = readAttemptNumberInternal(filename);
        return numTries >= 0 ? numTries : 0;
    }

    /**
     * Returns how many times we've tried to upload a certain minidump file.
     * @return The number of attempts to upload the given minidump file, parsed from its filename,
     *     Returns -1 if an attempt number cannot be parsed from the filename.
     */
    @VisibleForTesting
    static int readAttemptNumberInternal(String filename) {
        int tryIndex = filename.lastIndexOf(UPLOAD_ATTEMPT_DELIMITER);
        if (tryIndex >= 0) {
            tryIndex += UPLOAD_ATTEMPT_DELIMITER.length();
            String numTriesString = filename.substring(tryIndex);
            Scanner numTriesScanner = new Scanner(numTriesString).useDelimiter("[^0-9]+");
            try {
                int nextInt = numTriesScanner.nextInt();
                // Only return the number if it occurs just after the UPLOAD_ATTEMPT_DELIMITER.
                return numTriesString.indexOf(Integer.toString(nextInt)) == 0 ? nextInt : -1;
            } catch (NoSuchElementException e) {
                return -1;
            }
        }
        return -1;
    }

    /**
     * Marks a crash dump file as successfully uploaded, by renaming the file.
     *
     * Does not immediately delete the file, for testing reasons. However, if renaming fails,
     * attempts to delete the file immediately.
     */
    public static void markUploadSuccess(File crashDumpFile) {
        CrashFileManager.renameCrashDumpFollowingUpload(crashDumpFile, UPLOADED_MINIDUMP_SUFFIX);
    }

    /**
     * Marks a crash dump file's upload being skipped. An upload might be skipped due to lack of
     * user consent, or due to this client being excluded from the sample of clients reporting
     * crashes.
     *
     * Renames the file rather than deleting it, so that the user can manually upload the file later
     * (via chrome://crashes). However, if renaming fails, attempts to delete the file immediately.
     */
    public static void markUploadSkipped(File crashDumpFile) {
        CrashFileManager.renameCrashDumpFollowingUpload(
                crashDumpFile, UPLOAD_SKIPPED_MINIDUMP_SUFFIX);
    }

    /**
     * Renames a crash dump file. However, if renaming fails, attempts to delete the file
     * immediately.
     */
    private static void renameCrashDumpFollowingUpload(File crashDumpFile, String suffix) {
        // The pre-upload filename might have been either "foo.dmpN.tryM" or "foo.forcedN.tryM".
        String newName = crashDumpFile.getPath()
                                 .replace(NOT_YET_UPLOADED_MINIDUMP_SUFFIX, suffix)
                                 .replace(UPLOAD_FORCED_MINIDUMP_SUFFIX, suffix);
        boolean renamed = crashDumpFile.renameTo(new File(newName));
        if (!renamed) {
            Log.w(TAG, "Failed to rename " + crashDumpFile);
            if (!crashDumpFile.delete()) {
                Log.w(TAG, "Failed to delete " + crashDumpFile);
            }
        }
    }

    private final File mCacheDir;

    public CrashFileManager(File cacheDir) {
        if (cacheDir == null) {
            throw new NullPointerException("Specified context cannot be null.");
        } else if (!cacheDir.isDirectory()) {
            throw new IllegalArgumentException(cacheDir.getAbsolutePath() + " is not a directory.");
        }
        mCacheDir = cacheDir;
    }

    /**
     * Create the crash directory for this file manager unless it exists already.
     * @return true iff the crash directory exists when this method returns.
     */
    private boolean ensureCrashDirExists() {
        File crashDir = getCrashDirectory();
        // Call mkdir before isDirectory to ensure that if another thread created the directory
        // just before the call to mkdir, the current thread fails mkdir, but passes isDirectory.
        return crashDir.mkdir() || crashDir.isDirectory();
    }

    /**
     * @return whether the crash directory already exists.
     */
    public boolean crashDirectoryExists() {
        return getCrashDirectory().isDirectory();
    }

    /**
     * Imports minidumps from Crashpad's database to the Crash Reports directory, converting them to
     * MIME files.
     **/
    private void importCrashpadMinidumps() {
        File crashpadDir = getCrashpadDirectory();
        if (crashpadDir.exists() && ensureCrashDirExists()) {
            File crashDir = getCrashDirectory();
            CrashReportMimeWriter.rewriteMinidumpsAsMIMEs(crashpadDir, crashDir);
        }
    }

    /**
     * Imports minidumps from Crashpad's database to the Crash Reports directory, converting them to
     * MIME files and returning crash info as key-value pairs.
     *
     * @return a Map for crash report uuid to this crash info key-value pairs.
     */
    public Map<String, Map<String, String>> importMinidumpsCrashKeys() {
        File crashpadDir = getCrashpadDirectory();
        if (!crashpadDir.exists() || !ensureCrashDirExists()) {
            return null;
        }
        File crashDir = getCrashDirectory();
        return CrashReportMimeWriter.rewriteMinidumpsAsMIMEsAndGetCrashKeys(
                crashpadDir, crashDir);
    }

    /**
     * Returns the most recent minidump without a logcat for a given pid, or null if no such
     * minidump exists. This method begins by reading all minidumps from Crashpad's database and
     * rewriting them as MIME files in the Crash Reports directory.
     */
    public File getMinidumpSansLogcatForPid(int pid) {
        importCrashpadMinidumps();
        File[] foundFiles = listCrashFiles(
            Pattern.compile("\\.dmp" + Integer.toString(pid) + "\\z"));
        return foundFiles.length > 0 ? foundFiles[0] : null;
    }

    /**
     * Returns all minidump files that definitely do not have logcat output, sorted by modification
     * time stamp. This method begins by reading all minidumps from Crashpad's database and
     * rewriting them as MIME files in the Crash Reports directory. Note: This method does not
     * provide an "if and only if" test: it may return some files that lack logcat output, if logcat
     * output has been intentionally skipped for those minidumps. However, any files returned
     * definitely lack logcat output.
     */
    public File[] getMinidumpsSansLogcat() {
        importCrashpadMinidumps();
        return listCrashFiles(MINIDUMP_SANS_LOGCAT_PATTERN);
    }

    /**
     * Returns all minidump files currently in the Crash Reports directory that definitely do not
     * have logcat output, sorted by modification time stamp. Note: This method does not provide an
     * "if and only if" test: it may return some files that lack logcat output, if logcat outpuy has
     * been intentionally skipped for those minidumps. However, any files returned definitely lack
     * logcat output.
     */
    public File[] getCurrentMinidumpsSansLogcat() {
        return listCrashFiles(MINIDUMP_SANS_LOGCAT_PATTERN);
    }

    /**
     * Returns all minidump files that could still be uploaded, sorted by modification time stamp.
     * Only returns files that we have tried to upload less than {@param maxTries} number of times.
     */
    public File[] getMinidumpsReadyForUpload(int maxTries) {
        return getFilesBelowMaxTries(
                listCrashFiles(MINIDUMP_ALL_READY_FOR_UPLOAD_PATTERN), maxTries);
    }

    /**
     * Returns minidump files that could still be uploaded excluding forced uploads,
     * sorted by modification time stamp.
     */
    public File[] getMinidumpsNotForcedReadyForUpload() {
        return listCrashFiles(MINIDUMP_READY_FOR_UPLOAD_PATTERN);
    }

    /**
     * Returns all minidump files that could still be uploaded, sorted by modification time stamp.
     */
    public File[] getMinidumpsSkippedUpload() {
        return listCrashFiles(MINIDUMP_SKIPPED_UPLOAD_PATTERN);
    }

    /**
     * Returns minidump files that are forced to be uploaded by the user, sorted by modification
     * time stamp.
     */
    public File[] getMinidumpsForcedUpload() {
        return listCrashFiles(MINIDUMP_FORCED_UPLOAD_PATTERN);
    }

    /**
     * Returns all minidump files with the uid {@param uid} from {@param minidumpFiles}.
     */
    public static List<File> filterMinidumpFilesOnUid(File[] minidumpFiles, int uid) {
        List<File> uidMinidumps = new ArrayList<>();
        for (File minidump : minidumpFiles) {
            if (belongsToUid(minidump, uid)) {
                uidMinidumps.add(minidump);
            }
        }
        return uidMinidumps;
    }

    public void cleanOutAllNonFreshMinidumpFiles() {
        for (File f : getAllUploadedFiles()) {
            deleteFile(f);
        }
        for (File f : getAllTempFiles()) {
            deleteFile(f);
        }

        int numSavedCrashes = 0;
        for (File f : listCrashFiles(null)) {
            // The uploads.log file should always be preserved, as it stores the metadata that
            // powers the chrome://crashes UI.
            if (f.getName().equals(CRASH_DUMP_LOGFILE)) {
                continue;
            }

            // Delete any crash reports that are especially old.
            long ageInMillis = new Date().getTime() - f.lastModified();
            long ageInDays = TimeUnit.DAYS.convert(ageInMillis, TimeUnit.MILLISECONDS);
            if (ageInDays > MAX_CRASH_REPORT_AGE_IN_DAYS) {
                deleteFile(f);
                continue;
            }

            // Delete the oldest crash reports that exceed the cap on the number of allowed reports.
            if (numSavedCrashes < MAX_CRASH_REPORTS_TO_KEEP) {
                // Note that /not/ deleting the file is a no-op, so all that's needed is to mark
                // that one more file has been kept.
                ++numSavedCrashes;
            } else {
                deleteFile(f);
            }
        }
    }

    /**
     * Filters a set of files to keep the ones we have tried to upload only a few times.
     * Given a set of files {@param unfilteredFiles}, returns only the files in that set which we
     * have tried to upload less than {@param maxTries} times.
     */
    @VisibleForTesting
    static File[] getFilesBelowMaxTries(File[] unfilteredFiles, int maxTries) {
        List<File> filesBelowMaxTries = new ArrayList<>();
        for (File file : unfilteredFiles) {
            if (readAttemptNumber(file.getName()) < maxTries) {
                filesBelowMaxTries.add(file);
            }
        }
        return filesBelowMaxTries.toArray(new File[filesBelowMaxTries.size()]);
    }

    /**
     * Returns a sorted and filtered list of files within the crash directory.
     */
    @VisibleForTesting
    File[] listCrashFiles(@Nullable final Pattern pattern) {
        File crashDir = getCrashDirectory();

        FilenameFilter filter = null;
        if (pattern != null) {
            filter = new FilenameFilter() {
                @Override
                public boolean accept(File dir, String filename) {
                    return pattern.matcher(filename).find();
                }
            };
        }
        File[] foundFiles = crashDir.listFiles(filter);
        if (foundFiles == null) {
            Log.w(TAG, crashDir.getAbsolutePath() + " does not exist or is not a directory");
            return new File[] {};
        }
        Arrays.sort(foundFiles, sFileComparator);
        return foundFiles;
    }

    @VisibleForTesting
    public File[] getAllUploadedFiles() {
        return listCrashFiles(UPLOADED_MINIDUMP_PATTERN);
    }

    @VisibleForTesting
    public File getCrashDirectory() {
        return new File(mCacheDir, CRASH_DUMP_DIR);
    }

    private File getCrashpadDirectory() {
        return new File(mCacheDir, CRASHPAD_DIR);
    }

    public File createNewTempFile(String name) throws IOException {
        File f = new File(getCrashDirectory(), name);
        if (f.exists()) {
            if (f.delete()) {
                f = new File(getCrashDirectory(), name);
            } else {
                Log.w(TAG, "Unable to delete previous logfile" + f.getAbsolutePath());
            }
        }
        return f;
    }

    /**
     * @return the crash file named {@param filename}.
     */
    public File getCrashFile(String filename) {
        return new File(getCrashDirectory(), filename);
    }

    /**
     * Returns the minidump file with the given local ID, or null if no minidump file has the given
     * local ID.
     * NOTE: Crash files that have already been successfully uploaded are not included.
     *
     * @param localId The local ID of the crash report.
     * @return The matching File, or null if no matching file is found.
     */
    public File getCrashFileWithLocalId(String localId) {
        for (File f : listCrashFiles(null)) {
            // Only match non-uploaded or previously skipped files. In particular, do not match
            // successfully uploaded files; nor files which are not minidump files, such as logcat
            // files.
            if (!f.getName().contains(NOT_YET_UPLOADED_MINIDUMP_SUFFIX)
                    && !f.getName().contains(UPLOAD_SKIPPED_MINIDUMP_SUFFIX)
                    && !f.getName().contains(UPLOAD_FORCED_MINIDUMP_SUFFIX)) {
                continue;
            }

            String filenameSansExtension = f.getName().split("\\.")[0];
            if (filenameSansExtension.endsWith(localId)) {
                return f;
            }
        }
        return null;
    }

    /**
     * Extracts crash local ID from crash file name.
     *
     * ID is the last part of the file name. e.g. {@code
     * chromium-renderer-minidump-f297dbcba7a2d0bb.dump.try2} has local ID of {@code
     * f297dbcba7a2d0bb}.
     *
     * @param fileName Crash File name.
     * @return Local ID string or null if not found.
     */
    public static String getCrashLocalIdFromFileName(String fileName) {
        Matcher matcher = CRASH_LOCAL_ID_PATTERN.matcher(fileName);
        if (matcher.find()) {
            return matcher.group(1);
        }
        return null;
    }

    /**
     * @return the file used for logging crash upload events.
     */
    public File getCrashUploadLogFile() {
        return new File(getCrashDirectory(), CRASH_DUMP_LOGFILE);
    }

    @VisibleForTesting
    File[] getAllTempFiles() {
        return listCrashFiles(TMP_PATTERN);
    }

    /**
     * Delete the oldest minidump if we have reached our threshold on the number of minidumps to
     * store (either per-app, or globally).
     * @param uid The uid of the app to check the minidump limit for.
     */
    private void enforceMinidumpStorageRestrictions(int uid) {
        File[] allMinidumpFiles = listCrashFiles(MINIDUMP_ALL_READY_FOR_UPLOAD_PATTERN);
        List<File> minidumpFilesWithCurrentUid = filterMinidumpFilesOnUid(allMinidumpFiles, uid);

        // If we have exceeded our cap per uid, delete the oldest minidump of the same uid
        if (minidumpFilesWithCurrentUid.size() >= MAX_CRASH_REPORTS_TO_UPLOAD_PER_UID) {
            // Minidumps are sorted from newest to oldest.
            File oldestFile =
                    minidumpFilesWithCurrentUid.get(minidumpFilesWithCurrentUid.size() - 1);
            if (!oldestFile.delete()) {
                // Note that we will still try to copy the new file if this deletion fails.
                Log.w(TAG, "Couldn't delete old minidump " + oldestFile.getAbsolutePath());
            }
            return;
        }

        // If we have exceeded our minidump cap, delete the oldest minidump.
        if (allMinidumpFiles.length >= MAX_CRASH_REPORTS_TO_UPLOAD) {
            // Minidumps are sorted from newest to oldest.
            File oldestFile = allMinidumpFiles[allMinidumpFiles.length - 1];
            if (!oldestFile.delete()) {
                // Note that we will still try to copy the new file if this deletion fails.
                Log.w(TAG, "Couldn't delete old minidump " + oldestFile.getAbsolutePath());
            }
        }
    }

    /**
     * Copy a minidump from the File Descriptor {@param fd}.
     * Use {@param tmpDir} as an intermediate location to store temporary files.
     * @return The new minidump file copied with the contents of the File Descriptor, or null if the
     *         copying failed.
     */
    public File copyMinidumpFromFD(FileDescriptor fd, File tmpDir, int uid) throws IOException {
        File crashDirectory = getCrashDirectory();
        if (!ensureCrashDirExists()) {
            Log.e(TAG, "Crash directory doesn't exist");
            return null;
        }
        // Only threads copying minidumps will be touching this tmp-directory. Since these threads
        // are synchronized to avoid copying several minidumps simultaneously we don't need
        // synchronization explicitly for creating this tmp-directory.
        if (!tmpDir.isDirectory() && !tmpDir.mkdir()) {
            Log.e(TAG, "Couldn't create " + tmpDir.getAbsolutePath());
            return null;
        }
        if (tmpDir.getCanonicalPath().equals(crashDirectory.getCanonicalPath())) {
            // Cause a hard failure since this should never happen in the wild.
            throw new RuntimeException("The tmp-dir and the crash dir can't have the same paths.");
        }

        enforceMinidumpStorageRestrictions(uid);

        // Make sure the temp file doesn't overwrite an existing file.
        File tmpFile = createMinidumpTmpFile(tmpDir);
        FileInputStream in = null;
        FileOutputStream out = null;
        // TODO(gsennton): ensure that the copied file is indeed a minidump.
        try {
            in = new FileInputStream(fd);
            out = new FileOutputStream(tmpFile);
            final int bufSize = 4096;
            byte[] buf = new byte[bufSize];
            final int maxSize = 1024 * 1024; // 1MB maximum size
            int curCount = in.read(buf);
            int totalCount = curCount;
            while ((curCount != -1) && (totalCount < maxSize)) {
                out.write(buf, 0, curCount);
                curCount = in.read(buf);
                totalCount += curCount;
            }
            if (curCount != -1) {
                // We are trying to keep on reading beyond our maximum threshold (1MB) - bail!
                Log.w(TAG, "Tried to copy a file of size > 1MB, deleting the file and bailing!");
                if (!tmpFile.delete()) {
                    Log.w(TAG, "Couldn't delete file " + tmpFile.getAbsolutePath());
                }
                return null;
            }
        } finally {
            try {
                if (out != null) out.close();
            } catch (IOException e) {
                Log.w(TAG, "Couldn't close minidump output stream ", e);
            }
            try {
                if (in != null) in.close();
            } catch (IOException e) {
                Log.w(TAG, "Couldn't close minidump input stream ", e);
            }
        }
        File minidumpFile = new File(crashDirectory, createUniqueMinidumpNameForUid(uid));
        if (tmpFile.renameTo(minidumpFile)) {
            return minidumpFile;
        }
        return null;
    }

    /**
     * Returns whether the {@param minidump} belongs to the uid {@param uid}.
     */
    private static boolean belongsToUid(File minidump, int uid) {
        return minidump.getName().startsWith(uid + UID_DELIMITER);
    }

    /**
     * Returns a unique minidump name based on {@param uid} to differentiate between minidumps from
     * different packages.
     * The 'uniqueness' of the file name lies in it being created from a UUID. A UUID is a
     * Universally Unique ID - it is simply a 128-bit value that can be used to uniquely identify
     * some entity. A uid, on the other hand, is a unique identifier for Android packages.
     */
    private static String createUniqueMinidumpNameForUid(int uid) {
        return uid + UID_DELIMITER + UUID.randomUUID() + NOT_YET_UPLOADED_MINIDUMP_SUFFIX
                + READY_FOR_UPLOAD_SUFFIX;
    }

    /**
     * Create a temporary file to store a minidump in before renaming it with a real minidump name.
     * @return a new temporary file with prefix {@param prefix} stored in the directory
     * {@param directory}.
     *
     */
    private static File createMinidumpTmpFile(File directory) throws IOException {
        return File.createTempFile("webview_minidump", TMP_SUFFIX, directory);
    }
}
