// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.StaleObjectException;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject2;

import org.chromium.base.test.util.TimeoutTimer;

import java.util.ArrayList;
import java.util.List;

/**
 * Convenience helper for IUi2Locators, with retry capability to reduce flakes.
 *
 * IUi2Locators contain matching criteria to find UI nodes on a device, but it
 * lacks the ability to retry on failures.  They also do not give directly useful
 * information such as the text displayed in an UI node that tests will likely need.
 * This helper class provides these capabilities.
 */
public class UiLocatorHelper {
    private static final long DEFAULT_TIMEOUT_MS = 3000L;
    // UI_CHECK_INTERVAL_MS is intentionally not modifiable so that longer timeouts
    // don't lead to slowness due to the checking interval being too coarse.
    static final long UI_CHECK_INTERVAL_MS = DEFAULT_TIMEOUT_MS / 4L;

    private static final ElementConverter<String> CONVERTER_TEXT =
            object2 -> {
                return object2.getText();
            };

    private static final ElementConverter<String> CONVERTER_DESC =
            object2 -> {
                return object2.getContentDescription();
            };

    private static final ElementConverter<Boolean> CONVERTER_CHECKED =
            object2 -> {
                if (object2.isCheckable()) {
                    return object2.isChecked();
                } else {
                    throw new UiLocationException("Item in " + object2 + " is not checkable.");
                }
            };

    private final UiDevice mDevice;
    private long mTimeout;

    /** Create a UiLocatorHelper with default timeout. */
    UiLocatorHelper() {
        this(DEFAULT_TIMEOUT_MS);
    }

    /**
     * Create a UiLocatorHelper with specified timeout.
     * @param timeout Timeout in milliseconds.
     */
    UiLocatorHelper(long timeout) {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mTimeout = timeout;
    }

    /**
     * Determines if a node is found on the screen.
     * @param locator Locator used to find the node.
     * @return        true if node is found, false otherwise.
     */
    public boolean isOnScreen(@NonNull IUi2Locator locator) {
        try {
            getOne(locator);
        } catch (UiLocationException e) {
            return false;
        }
        return true;
    }

    /**
     * Determines if a node is found on the screen, does not retry.
     * @param locator Locator used to find the node.
     * @param root    Node to search under, or null if all the nodes should be searched.
     *                Possible root staleness will make retries ineffective, this means
     *                that it is only suitable for immediate methods to take a root node.
     * @return        true if node is found, false otherwise.
     */
    public boolean isOnScreenImmediate(@NonNull IUi2Locator locator, UiObject2 root) {
        return getOneImmediate(locator, root) == null;
    }

    /**
     * Checks that a node is found on the screen.
     * @param locator Locator used to find the node.
     * @throws        UiLocationException if locator is not found on screen.
     */
    public void verifyOnScreen(@NonNull IUi2Locator locator) {
        getOne(locator);
    }

    /**
     * Get text from a single node using locator.
     * @param locator          Locator used to find the nodes.
     * @return                 Text field of the node.
     * @throws UiLocationException if locator didn't find any nodes within timeout interval.
     */
    public String getOneText(@NonNull IUi2Locator locator) {
        return getOneElement(locator, CONVERTER_TEXT);
    }

    /**
     * Get list of text using locator.
     * @param locator          Locator used to find the nodes.
     * @return                 List of text.
     * @throws UiLocationException if locator didn't find any nodes within timeout interval.
     */
    public List<String> getAllTexts(@NonNull IUi2Locator locator) {
        return getAllElements(locator, CONVERTER_TEXT);
    }

    /**
     * Get the node's text without retries.
     * @param locator Locator used to find the nodes.
     * @param root    Node to search under, or null if all the nodes should be searched.
     *                Possible root staleness will make retries ineffective, this means
     *                that it is only suitable for immediate methods to take a root node.
     * @return        Text field of the node, or null if locator didn't find any nodes.
     */
    public String getOneTextImmediate(@NonNull IUi2Locator locator, @Nullable UiObject2 root) {
        return getOneElementImmediate(locator, root, CONVERTER_TEXT);
    }

    /**
     * Get list of text using locator without retries.
     * @param locator Locator used to find the nodes.
     * @param root    Node to search under, or null if all the nodes should be searched.
     *                Possible root staleness will make retries ineffective, this means
     *                that it is only suitable for immediate methods to take a root node.
     * @return        List of text, or empty list if locator didn't find any nodes.
     */
    public List<String> getAllTextsImmediate(
            @NonNull IUi2Locator locator, @Nullable UiObject2 root) {
        return getAllElementsImmediate(locator, root, CONVERTER_TEXT);
    }

    /**
     * Get content description from a single node using locator.
     * @param locator          Locator used to find the nodes.
     * @return                 Content description field of the node.
     * @throws UiLocationException if locator didn't find any nodes within timeout interval.
     */
    public String getOneDescription(@NonNull IUi2Locator locator) {
        return getOneElement(locator, CONVERTER_DESC);
    }

    /**
     * Get list of content descriptions using locator.
     * @param locator          Locator used to find the nodes.
     * @return                 List of content descriptions.
     * @throws UiLocationException if locator didn't find any nodes within timeout interval.
     */
    public List<String> getAllDescriptions(@NonNull IUi2Locator locator) {
        return getAllElements(locator, CONVERTER_DESC);
    }

    /**
     * Get the node's content description without retries.
     * @param locator Locator used to find the nodes.
     * @param root    Node to search under, or null if all the nodes should be searched.
     *                Possible root staleness will make retries ineffective, this means
     *                that it is only suitable for immediate methods to take a root node.
     * @return        Content description of the node, or null if locator didn't find any nodes.
     */
    public String getOneDescriptionImmediate(
            @NonNull IUi2Locator locator, @Nullable UiObject2 root) {
        return getOneElementImmediate(locator, root, CONVERTER_DESC);
    }

    /**
     * Get list of content descriptions using locator without retries.
     * @param locator Locator used to find the nodes.
     * @param root    Node to search under, or null if all the nodes should be searched.
     *                Possible root staleness will make retries ineffective, this means
     *                that it is only suitable for immediate methods to take a root node.
     * @return        List of content descriptions, or empty list if locator didn't find any nodes.
     */
    public List<String> getAllDescriptionsImmediate(
            @NonNull IUi2Locator locator, @Nullable UiObject2 root) {
        return getAllElementsImmediate(locator, root, CONVERTER_DESC);
    }

    /**
     * Get checked status from a single node using locator.
     * @param locator           Locator used to find the nodes.
     * @return                  checked status of the node.
     * @throws  UiLocationException if locator didn't find any nodes within timeout interval.
     */
    public Boolean getOneChecked(@NonNull IUi2Locator locator) {
        return getOneElement(locator, CONVERTER_CHECKED);
    }

    /**
     * Get list of checked statuses using locator.
     * @param locator          Locator used to find the nodes.
     * @return                 List of checked statuses.
     * @throws UiLocationException if locator didn't find any nodes within timeout interval.
     */
    public List<Boolean> getAllChecked(@NonNull IUi2Locator locator) {
        return getAllElements(locator, CONVERTER_CHECKED);
    }

    /**
     * Get the node's checked status without retries.
     * @param locator Locator used to find the nodes.
     * @param root    Node to search under, or null if all the nodes should be searched.
     *                Possible root staleness will make retries ineffective, this means
     *                that it is only suitable for immediate methods to take a root node.
     * @return        Checked status of the node, or null if locator didn't find any nodes.
     */
    public Boolean getOneCheckedImmediate(@NonNull IUi2Locator locator, @Nullable UiObject2 root) {
        return getOneElementImmediate(locator, root, CONVERTER_CHECKED);
    }

    /**
     * Get list of checked statuses using locator without retries.
     * @param locator Locator used to find the nodes.
     * @param root    Node to search under, or null if all the nodes should be searched.
     *                Possible root staleness will make retries ineffective, this means
     *                that it is only suitable for immediate methods to take a root node.
     * @return        List of checked statuses, or empty list if locator didn't find any nodes.
     */
    public List<Boolean> getAllCheckedImmediate(
            @NonNull IUi2Locator locator, @Nullable UiObject2 root) {
        return getAllElementsImmediate(locator, root, CONVERTER_CHECKED);
    }

    /**
     * Returns the first element found using locator.
     * Throws UiLocationException if not found.
     * @param locator Locator to use to find the element.
     * @return        UiObject2
     */
    public UiObject2 getOne(@NonNull IUi2Locator locator) {
        List<UiObject2> all = getAll(locator);
        return all.get(0);
    }

    /**
     * Returns the first element found using locator.
     * Could return null but does not throw.
     * @param locator Locator to use to find the element.
     * @param root    Search for elements within root, or on the device if null.
     *                Possible root staleness will make retries ineffective, this means
     *                that it is only suitable for immediate methods to take a root node.
     * @return        UiObject2
     */
    public UiObject2 getOneImmediate(@NonNull IUi2Locator locator, @Nullable UiObject2 root) {
        List<UiObject2> all = getAllImmediate(locator, root);
        if (all.size() == 0) {
            return null;
        } else {
            return all.get(0);
        }
    }

    /**
     * Return a list of UiObject2 nodes matching locator, will retry up to timeout.
     * @param locator          Locator to use to find the element.
     * @return                 list of elements matching the locator
     * @throws UiLocationException if locator didn't find any nodes.
     */
    public List<UiObject2> getAll(IUi2Locator locator) {
        TimeoutTimer timeoutTimer = new TimeoutTimer(mTimeout);
        int attempts = 0;
        List<UiObject2> object2s = null;
        while (true) {
            try {
                object2s = getAllInternal(locator, null);
            } catch (StaleObjectException | NullPointerException e) {
            } finally {
                attempts++;
            }
            if (object2s != null && object2s.size() != 0) {
                return object2s;
            }
            if (timeoutTimer.isTimedOut()) {
                break;
            }
            Utils.sleep(UI_CHECK_INTERVAL_MS);
        }
        throw new UiLocationException(
                "Could not find any objects after "
                        + mTimeout
                        + "ms and "
                        + attempts
                        + " attempts.",
                locator);
    }

    /**
     * Return a list of UiObject2 nodes matching locator, without retries.
     * @param locator Locator to use to find the element.
     * @param root    the root element to search under, or null to search for any
     *                node on the device.
     *                Possible root staleness will make retries ineffective, this means
     *                that it is only suitable for immediate methods to take a root node.
     * @return        list of elements matching the locator, or empty if nothing
     *                matched.
     */
    public List<UiObject2> getAllImmediate(@NonNull IUi2Locator locator, UiObject2 root) {
        try {
            return getAllInternal(locator, root);
        } catch (StaleObjectException | NullPointerException e) {
            return new ArrayList<UiObject2>();
        }
    }

    /**
     * Delegate to be used with getCustomElements.
     * @param <T> The type of the element.
     */
    public static interface CustomElementMaker<T> {
        /**
         * Should construct an element given a node.
         * @param root          The input node.
         * @param isLastAttempt getCustomElements may call this delegate
         *                      multiple times if errors are thrown from this
         *                      method and timeout has not been reached.  If
         *                      isLastAttempt is true, then it indicates that
         *                      getCustomElements will not call this delegate
         *                      again.  For example the delegate can return null
         *                      on the lastAttempt if it still encounters errors
         *                      which indicates that a properly formed element is
         *                      not found on the page, this case could happen if
         *                      an element gets cutoff at a scroll boundary.
         * @return              The element if construction is successful, null
         *                      otherwise.
         * @throws UiLocationException Should throw a UiLocationException or
         *                      UiStaleObjectException if getCustomElements
         *                      should re-obtain a root using its provided
         *                      locator.
         */
        T makeElement(UiObject2 root, boolean isLastAttempt);
    }

    /**
     * Create a list of objects based on nodes found by locator.
     * @param locator Locator used to find the nodes.
     * @param maker   CustomElementMaker used to construct the custom element.
     * @param <T>     The type of the objects in the list.
     * @return        List of constructed objects of type T.
     */
    public <T> List<T> getCustomElements(
            @NonNull IUi2Locator locator, @NonNull CustomElementMaker<T> maker) {
        List<T> elements = new ArrayList<>();
        List<UiObject2> roots;
        boolean isLastAttempt = false;

        TimeoutTimer lastAttemptTimer = new TimeoutTimer(mTimeout - UI_CHECK_INTERVAL_MS);
        while (true) {
            try {
                try {
                    roots = getAllInternal(locator, null);
                } catch (UiLocationException e) {
                    return elements;
                }
                for (UiObject2 root : roots) {
                    T e = maker.makeElement(root, isLastAttempt);
                    if (e != null) {
                        elements.add(e);
                    }
                }
                return elements;
            } catch (StaleObjectException | UiLocationException e) {
                if (isLastAttempt) {
                    throw e;
                }
                // makeElement could throw while going through the list of roots,
                // so clear out any elements to avoid duplicates and stale ones.
                elements.clear();
                Utils.sleep(UI_CHECK_INTERVAL_MS);
                // If the next interaction will cause timeout to be exceeded, then
                // flag isLastAttempt so client can choose to perform a work-around
                // instead of throwing an exception again.
                if (lastAttemptTimer.isTimedOut()) {
                    isLastAttempt = true;
                }
            }
        }
    }

    /**
     * Define a conversion method creates an object from info in a UiObject2 node.
     * @param <T> Type of the object.
     */
    private static interface ElementConverter<T> {
        T convert(UiObject2 object2);
    }

    private <T> T getOneElement(IUi2Locator locator, ElementConverter<T> converter) {
        List<T> all = getAllElements(locator, converter);
        if (all.size() > 0) {
            return all.get(0);
        } else {
            return null;
        }
    }

    private <T> T getOneElementImmediate(
            IUi2Locator locator, UiObject2 root, ElementConverter<T> converter) {
        List<T> all = getAllElementsImmediate(locator, root, converter);
        return Utils.nullableGet(all, 0);
    }

    private <T> List<T> getAllElementsImmediate(
            IUi2Locator locator, UiObject2 root, ElementConverter<T> elementConverter) {
        List<UiObject2> all = getAllImmediate(locator, root);
        return convertAll(elementConverter, all);
    }

    private <T> List<T> getAllElements(IUi2Locator locator, ElementConverter<T> elementConverter) {
        List<UiObject2> all = getAll(locator);
        return convertAll(elementConverter, all);
    }

    // Convert each item in all and return the resultant list.
    private <T> List<T> convertAll(ElementConverter<T> converter, List<UiObject2> all) {
        List<T> allT = new ArrayList<>();
        for (UiObject2 object2 : all) {
            allT.add(converter.convert(object2));
        }
        return allT;
    }

    /**
     * Returns all nodes that matched the locator.
     * Note that StaleObject or NPE may be thrown from this if the UI has gotten into an
     * inconsistent state, this usually means the caller should retry the operation.
     * @param locator Locator to use.
     * @param root    Root node to match under, or null to match on any node.
     * @return        List of matched nodes, possibly empty.
     */
    private List<UiObject2> getAllInternal(@NonNull IUi2Locator locator, UiObject2 root) {
        return root == null ? locator.locateAll(mDevice) : locator.locateAll(root);
    }
}
