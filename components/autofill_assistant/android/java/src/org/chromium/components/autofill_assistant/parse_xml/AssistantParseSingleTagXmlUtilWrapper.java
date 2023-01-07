// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.parse_xml;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.autofill_assistant.guided_browsing.parse_xml.AssistantParseSingleTagXmlUtil;

/**
 * Util to expose parsing single tag XML functionality from the |guided_browsing| component to the
 * native code.
 */
@JNINamespace("autofill_assistant")
public class AssistantParseSingleTagXmlUtilWrapper {
    /**
     * Checks if given XML is signed or not.
     *
     * Note: We currently check for a XML being signed by simply checking if it only contains a
     * plain numerical string. This may lead to mis-classification when the input data is a random
     * numeric string.
     */
    @CalledByNative
    private static boolean isXmlSigned(String xmlString) {
        return AssistantParseSingleTagXmlUtil.isXmlSigned(xmlString);
    }

    /**
     * Extracts the attribute values for the given |attributes| of |xmlString|.
     *  * If parsing is completed successfully, then it returns the required String array of the
     *    attribute values.
     *  * If XML could not be successfully parsed or data for all the attributes is not found,
     *    then it returns empty String array.
     */
    @CalledByNative
    private static String[] extractValuesFromSingleTagXml(String xmlString, String[] attributes) {
        return AssistantParseSingleTagXmlUtil.extractValuesFromSingleTagXml(xmlString, attributes);
    }
}
