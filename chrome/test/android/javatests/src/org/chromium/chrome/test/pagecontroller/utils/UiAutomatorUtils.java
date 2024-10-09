// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import android.content.Context;
import android.content.Intent;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject2;

import org.hamcrest.Matchers;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Callable;

/** Allows tests to perform UI actions. */
public class UiAutomatorUtils {
    private static final String TAG = "UiAutomatorUtils";
    private static final int SWIPE_STEPS_PER_SECOND = 200;
    private static final int MAX_SWIPES = 30;
    private static final float DEFAULT_SWIPE_SECONDS_PER_PAGE = 0.2f;
    private static final float DEFAULT_SWIPE_SCREEN_FRACTION = 0.6f;
    private static final long WAIT_TIMEOUT_MS = 20000L;
    private static final long UI_CHECK_INTERVAL = 1000L;
    private static final long SHORT_CLICK_DURATION = 10L;
    private static final long LONG_CLICK_DURATION = 1000L;
    // Give applications more time to launch.
    private static final long LAUNCH_TIMEOUT_MS = 9000L;
    // 100 steps corresponds to ~1 secs, this was determined
    // experimentally.  Internally uses UiDevice.drag to simulate
    // clicking, steps is one of the parameters to drag.
    public static final int CLICK_STEPS_PER_SECOND = 100;

    private UiDevice mDevice;
    private UiLocatorHelper mLocatorHelper;

    private static class LazyHolder {
        static final UiAutomatorUtils sInstance = new UiAutomatorUtils();
    }

    public static UiAutomatorUtils getInstance() {
        return LazyHolder.sInstance;
    }

    private UiAutomatorUtils() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mLocatorHelper = new UiLocatorHelper();
    }

    public String getText(UiObject2 root) {
        return root.getText();
    }

    /**
     * Launch application.
     * @param packageName Package name of the application.
     */
    public void launchApplication(String packageName) {
        Log.d(TAG, "Launching " + packageName);
        launchApplication(packageName, LAUNCH_TIMEOUT_MS);
    }

    /**
     * Stops the application.
     * @param packageName Package name of the application to stop.
     */
    public void stopApplication(String packageName) {
        Log.d(TAG, "Stopping " + packageName);
        try {
            executeShellCommand("am force-stop " + packageName);
        } catch (IOException e) {
            Log.d(TAG, "Failed to stop " + packageName);
            e.printStackTrace();
        }
    }

    public void pressBack() {
        mDevice.pressBack();
    }

    public void pressHome() {
        mDevice.pressHome();
    }

    public void pressRecentApps() throws RemoteException {
        mDevice.pressRecentApps();
    }

    /**
     * Takes device screenshot and saves it to screenShotFile.
     * @param screenShotFile Where the screenshot should be saved.
     */
    public void takeScreenShot(@NonNull File screenShotFile) {
        if (mDevice.takeScreenshot(screenShotFile)) {
            Log.d(TAG, "Screenshot successfully saved to " + screenShotFile.getAbsolutePath());
        } else {
            Log.e(TAG, "Screenshot unsuccessful " + screenShotFile.getAbsolutePath());
        }
    }

    /**
     * Performs click outside of the UI element found using locator.
     * The click will be centered in the rectangular screen area with the greatest
     * width or height that does not overlap with the UI element.
     * @param locator Locator to use to find the element.
     */
    public void clickOutsideOf(@NonNull IUi2Locator locator) {
        Rect bounds = getBounds(locator);
        Log.d(
                TAG,
                "Clicking outside of bounds with Bottom:"
                        + bounds.bottom
                        + " Top:"
                        + bounds.top
                        + " Left:"
                        + bounds.left
                        + " Right:"
                        + bounds.right);
        clickOutsideOfArea(bounds.left, bounds.top, bounds.right, bounds.bottom);
    }

    /** Get the UiLocatorHelper. */
    public UiLocatorHelper getLocatorHelper() {
        return mLocatorHelper;
    }

    /**
     * Get a copy of the UiLocatorHelper with a different timeout.
     * @param timeout The timeout in milliseconds.
     * @return UiLocatorHelper with the specified timeout.
     */
    public UiLocatorHelper getLocatorHelper(long timeout) {
        return new UiLocatorHelper(timeout);
    }

    /**
     * Performs a long click on node.
     * @param locator Locator to use to find the node.
     */
    public void longClick(@NonNull IUi2Locator locator) {
        clickDurationInternal(locator, LONG_CLICK_DURATION);
    }

    /**
     * Perform a click.
     * @param locator  Locator to find the UI element to click on.
     * @param duration Milliseconds that the click should last for.
     */
    private void clickDurationInternal(IUi2Locator locator, long duration) {
        UiObject2 object2 = mLocatorHelper.getOne(locator);
        Point center = object2.getVisibleCenter();
        mDevice.swipe(
                center.x,
                center.y,
                center.x,
                center.y,
                (int) (CLICK_STEPS_PER_SECOND * duration / 1000L));
    }

    /**
     * Get the rectangular bounds of the first UI element found using locator.
     * @param locator Locator used to find the UI element.
     * @return        Bounds of the UI element.
     */
    private Rect getBounds(@NonNull IUi2Locator locator) {
        UiObject2 object2 = mLocatorHelper.getOne(locator);
        return object2.getVisibleBounds();
    }

    /**
     * Copied over from UiAutomator UiDevice v18.0.1, it was removed for some reason, but is useful.
     * Executes a shell command using shell user identity, and return the standard output in string.
     *
     * <p>Calling function with large amount of output will have memory impacts, and the function
     * call will block if the command executed is blocking.
     *
     * <p>Note: calling this function requires API level 21 or above
     *
     * @param cmd Command to run
     * @return The standard output of the command
     * @since API Level 21
     */
    public String executeShellCommand(@NonNull String cmd) throws IOException {
        ParcelFileDescriptor pfd =
                InstrumentationRegistry.getInstrumentation()
                        .getUiAutomation()
                        .executeShellCommand(cmd);
        byte[] buf = new byte[512];
        int bytesRead;
        FileInputStream fis = new ParcelFileDescriptor.AutoCloseInputStream(pfd);
        StringBuilder stdout = new StringBuilder();
        while ((bytesRead = fis.read(buf)) != -1) {
            stdout.append(new String(buf, 0, bytesRead));
        }
        fis.close();
        return stdout.toString();
    }

    /**
     * Click on a node.
     * @param locator           Locator used to find the node.
     * @throws  UiLocationException If locator didn't find any nodes within timeout interval.
     */
    public void click(@NonNull IUi2Locator locator) {
        clickDurationInternal(locator, SHORT_CLICK_DURATION);
    }

    /**
     * Click on a node and checks for an expected outcome.
     * @param locator          Locator used to find the node to click on.
     * @param outcomeLocator   Locator to check for existence after the click.
     * @throws UiLocationException If locator didn't find any nodes within timeout interval or if
     *                         provided outcomeLocator didn't find any nodes after the click.
     */
    public void click(@NonNull IUi2Locator locator, @NonNull IUi2Locator outcomeLocator) {
        clickInternal(locator, outcomeLocator);
    }

    /**
     * Enters text in a node and press enter.
     * @param locator          Locator used to find the node.
     * @param text             The text to enter.
     * @throws UiLocationException If locator didn't find any nodes within timeout interval.
     */
    public void setTextAndEnter(@NonNull IUi2Locator locator, @NonNull String text) {
        click(locator);
        UiObject2 root = mLocatorHelper.getOne(locator);
        root.setText(text);
        mDevice.pressEnter();
    }

    /**
     * Performs the swipe up gesture repeatedly until a locator is found.
     *
     * @param locator locator that will stop the swipe if found on screen.
     * @param stopLocator locator that will cause an UiLocationException if found before locator.
     */
    public void swipeUpVerticallyUntilFound(IUi2Locator locator, IUi2Locator stopLocator) {
        swipeVerticallyUntilFound(locator, stopLocator, DEFAULT_SWIPE_SCREEN_FRACTION);
    }

    /**
     * Performs the swipe down gesture repeatedly until a locator is found.
     *
     * @param locator locator that will stop the swipe if found on screen.
     * @param stopLocator locator that will cause an UiLocationException if found before locator.
     */
    public void swipeDownVerticallyUntilFound(IUi2Locator locator, IUi2Locator stopLocator) {
        swipeVerticallyUntilFound(locator, stopLocator, -DEFAULT_SWIPE_SCREEN_FRACTION);
    }

    /**
     * Performs a swipe down gesture that's centered on the screen.
     * If the intention is to scroll to an element, consider using swipeDownVerticallyUntilFound.
     * @param fractionOfScreen The length of the swipe as a fraction of the screen, with 1 being
     *                         the max.
     */
    public void swipeDownVertically(float fractionOfScreen) {
        if (fractionOfScreen > 1 || fractionOfScreen <= 0) {
            throw new IllegalArgumentException("fractionOfScreen must be in the interval [0,1]");
        }
        swipeVertically(-fractionOfScreen);
    }

    /**
     * Performs a swipe up gesture that's centered on the screen.
     * If the intention is to scroll to an element, consider using swipeUpVerticallyUntilFound.
     * @param fractionOfScreen The length of the swipe as a fraction of the screen, with 1 being
     *                         the max.
     */
    public void swipeUpVertically(float fractionOfScreen) {
        if (fractionOfScreen > 1 || fractionOfScreen <= 0) {
            throw new IllegalArgumentException("fractionOfScreen must be in the interval [0,1]");
        }
        swipeVertically(fractionOfScreen);
    }

    /**
     * Performs a swipe gesture between 2 locators.
     * @param beginLocator Center the start of swipe on beginLocator.
     * @param endLocator   Center the end of swipe on endLocator.
     * @param duration     Time the swipe should take in milliseconds.
     */
    public void swipe(IUi2Locator beginLocator, IUi2Locator endLocator, float duration) {
        UiObject2 begin = mLocatorHelper.getOne(beginLocator);
        UiObject2 end = mLocatorHelper.getOne(endLocator);
        Point beginPoint = begin.getVisibleCenter();
        Point endPoint = end.getVisibleCenter();
        int steps = (int) (duration * SWIPE_STEPS_PER_SECOND / 1000);
        steps = steps > 0 ? steps : 1;
        mDevice.swipe(beginPoint.x, beginPoint.y, endPoint.x, endPoint.y, steps);
    }

    /**
     * Prints the UiAutomator window hierarchy to logcat.
     * @param message A leading message for the debug log.
     */
    public void printWindowHierarchy(String message) {
        try {
            List<String> hierarchy = getWindowHierarchy();
            Log.d(TAG, message);
            for (String line : hierarchy) {
                Log.d(TAG, line);
            }
        } catch (IOException e) {
            // Just log any errors and move on, testing can still continue.
            Log.e(TAG, "Printing hierarchy", e);
        }
    }

    public void waitUntilAnyVisible(IUi2Locator... locators) {
        CriteriaHelper.pollInstrumentationThread(
                toNotSatisfiedRunnable(
                        () -> {
                            for (IUi2Locator locator : locators) {
                                if (mLocatorHelper.isOnScreen(locator)) {
                                    return true;
                                }
                            }
                            return false;
                        },
                        "No Chrome views on screen. (i.e. Chrome has crashed "
                                + "on startup). Look at earlier logs for the actual "
                                + "crash stacktrace."),
                WAIT_TIMEOUT_MS,
                UI_CHECK_INTERVAL);
    }

    private static Runnable toNotSatisfiedRunnable(
            Callable<Boolean> criteria, String failureReason) {
        return () -> {
            try {
                boolean isSatisfied = criteria.call();
                Criteria.checkThat(failureReason, isSatisfied, Matchers.is(true));
            } catch (RuntimeException e) {
                throw e;
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        };
    }

    private void launchApplication(String packageName, long timeout) {
        Context context = ApplicationProvider.getApplicationContext();
        final Intent intent = context.getPackageManager().getLaunchIntentForPackage(packageName);
        if (intent == null) {
            throw new IllegalStateException(
                    "Could not get intent to launch "
                            + packageName
                            + ", please ensure that it is installed");
        }
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);

        IUi2Locator packageLocator = Ui2Locators.withPackageName(packageName);
        UiLocatorHelper helper = getLocatorHelper(timeout);
        helper.getOne(packageLocator);
    }

    // positive fraction indicates swipe up
    private void swipeVerticallyUntilFound(
            IUi2Locator locator, IUi2Locator stopLocator, float fractionOfScreen) {
        if (mLocatorHelper.isOnScreen(locator)) return;
        Utils.sleep(UiLocatorHelper.UI_CHECK_INTERVAL_MS);
        int iterationsLeft = MAX_SWIPES;
        while (!mLocatorHelper.isOnScreen(locator) && iterationsLeft-- > 0) {
            if (mLocatorHelper.isOnScreen(stopLocator)) {
                throw new UiLocationException(
                        "Did not find locator while swiping to " + stopLocator + ".", locator);
            }
            swipeVertically(fractionOfScreen);
            Utils.sleep(UiLocatorHelper.UI_CHECK_INTERVAL_MS);
        }
        if (!mLocatorHelper.isOnScreen(locator)) {
            throw new UiLocationException(
                    "Did not find locator after swiping for " + MAX_SWIPES + " times.", locator);
        }
    }

    private List<String> getWindowHierarchy() throws IOException {
        File tempFile = null;
        try {
            tempFile = getTempFile("temp_window_hierarchy", ".txt");
            mDevice.dumpWindowHierarchy(tempFile.getAbsolutePath());
            List<String> hierarchy = readAllFromFile(tempFile);
            List<String> formattedHiearchy = formatXml(hierarchy, 2);
            return formattedHiearchy;
        } finally {
            if (tempFile != null) {
                tempFile.delete();
            }
        }
    }

    // TODO(aluo): Refactor this to use standard libraries, see if Apache
    // xml-commons can be used for this.
    // Formats one-liner xml hierarchy dump into properly indented list of tags
    // to ease readability in logs.
    private List<String> formatXml(List<String> inputs, int indentSpaces) {
        StringBuilder inputBuilder = new StringBuilder();
        List<String> output = new ArrayList<>();

        for (String line : inputs) {
            inputBuilder.append(line.trim());
        }

        String xmlLine = inputBuilder.toString();
        int indent = 0;
        StringBuilder lineBuilder = new StringBuilder();
        int i;
        for (i = 0; i < xmlLine.length() - 1; i++) {
            char c = xmlLine.charAt(i);
            char nextC = xmlLine.charAt(i + 1);
            if (c == '<') {
                if (nextC == '/') {
                    indent--;
                }
                lineBuilder.append(new String(new char[indent * indentSpaces]).replace("\0", " "));
                lineBuilder.append(c);
                if (nextC != '/') {
                    indent++;
                }
            } else if (c == '>') {
                lineBuilder.append(c);
                output.add(lineBuilder.toString());
                lineBuilder.delete(0, lineBuilder.length());
            } else if (c == '/' && nextC == '>') {
                indent--;
                lineBuilder.append(c);
            } else {
                lineBuilder.append(c);
            }
        }
        if (xmlLine.length() > 0) {
            lineBuilder.append(xmlLine.charAt(i));
            output.add(lineBuilder.toString());
        }
        return output;
    }

    /**
     * Swipe screen vertically by fractions of screen height.
     * @param fractionOfScreen Amount to swipe by, -1 to 1.
     *                         Negative value swipes down, positive up.
     */
    private void swipeVertically(float fractionOfScreen) {
        int x = mDevice.getDisplayWidth() / 2;
        int h = mDevice.getDisplayHeight();
        int startY = h / 2 - (int) (fractionOfScreen / 2f * h);
        int stopY = startY + (int) (fractionOfScreen * h);
        int steps =
                (int)
                        (DEFAULT_SWIPE_SECONDS_PER_PAGE
                                * Math.abs(fractionOfScreen)
                                * SWIPE_STEPS_PER_SECOND);
        Log.d(
                TAG,
                "Swiping vertically from " + stopY + " to " + startY + " in " + steps + " steps");
        mDevice.swipe(x, stopY, x, startY, steps);
    }

    private void clickOutsideOfArea(int minX, int minY, int maxX, int maxY) {
        int screenHeight = mDevice.getDisplayHeight();
        int screenWidth = mDevice.getDisplayWidth();
        // Calculate horizontal and vertical margins from rect to screen's edge
        int leftMargin = minX;
        int rightMargin = screenWidth - maxX;
        int topMargin = minY;
        int bottomMargin = screenHeight - maxY;
        // Find the biggest margin value (widest or tallest)
        int maxMargin =
                Collections.max(Arrays.asList(rightMargin, leftMargin, topMargin, bottomMargin));
        // Click on the center of the area with the biggest margin value,
        // the order chosen here is arbitrary in case of a tie.
        if (maxMargin == rightMargin) {
            mDevice.click(maxX + rightMargin / 2, screenHeight / 2);
        } else if (maxMargin == leftMargin) {
            mDevice.click(leftMargin / 2, screenHeight / 2);
        } else if (maxMargin == topMargin) {
            mDevice.click(screenWidth / 2, topMargin / 2);
        } else if (maxMargin == bottomMargin) {
            mDevice.click(screenWidth / 2, screenHeight - bottomMargin / 2);
        }
    }

    private File getTempFile(String prefix, String suffix) throws IOException {
        File cacheDir = Environment.getExternalStorageDirectory();
        Log.d(TAG, "Create temp file in: " + cacheDir);
        Log.d(TAG, "My user id: " + Process.myUid());
        return File.createTempFile(prefix, suffix, cacheDir);
    }

    private void clickInternal(IUi2Locator locator, IUi2Locator outcomeLocator) {
        clickDurationInternal(locator, SHORT_CLICK_DURATION);
        if (outcomeLocator != null) {
            mLocatorHelper.getOne(outcomeLocator);
        }
        // If outcomeLocator is not specified, then caller intended not to check the
        // effect of the click here.
    }

    private List<String> readAllFromFile(File file) throws IOException {
        List<String> strings = new ArrayList<>();
        try (FileInputStream fileStream = new FileInputStream(file);
                InputStreamReader inputStream = new InputStreamReader(fileStream);
                BufferedReader streamReader = new BufferedReader(inputStream)) {
            String line;
            while ((line = streamReader.readLine()) != null) {
                strings.add(line);
            }
        }
        Log.d(
                TAG,
                "readAllFromFile read " + strings.size() + " lines from " + file.getAbsolutePath());
        return strings;
    }
}
