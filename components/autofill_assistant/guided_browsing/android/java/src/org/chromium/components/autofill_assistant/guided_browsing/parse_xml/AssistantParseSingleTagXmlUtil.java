// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.parse_xml;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;

import org.chromium.components.autofill_assistant.guided_browsing.GuidedBrowsingMetrics;
import org.chromium.components.autofill_assistant.guided_browsing.metrics.ParseSingleTagXmlActionEvent;

import java.io.IOException;
import java.io.StringReader;
import java.util.ArrayList;

/** Util for parsing single tag XML. */
public class AssistantParseSingleTagXmlUtil {
    /**
     * Checks if given XML is signed or not.
     *
     * Note: We currently check for a XML being signed by simply checking if it only contains a
     * plain numerical string. This may lead to mis-classification when the input data is a random
     * numeric string.
     */
    public static boolean isXmlSigned(String xmlString) {
        boolean isSigned = xmlString.matches("[0-9]+");
        if (isSigned) {
            GuidedBrowsingMetrics.recordParseSingleTagXmlActionEvent(
                    ParseSingleTagXmlActionEvent.SINGLE_TAG_XML_PARSE_SIGNED_DATA);
        }

        return isSigned;
    }

    /**
     * Extracts the attribute values for the given |attributes| of |xmlString|.
     *  * If parsing is completed successfully, then it returns the required String array of the
     *    attribute values.
     *  * If XML could not be successfully parsed or data for all the attributes is not found,
     *    then it returns empty String array.
     */
    public static String[] extractValuesFromSingleTagXml(String xmlString, String[] attributes) {
        GuidedBrowsingMetrics.recordParseSingleTagXmlActionEvent(
                ParseSingleTagXmlActionEvent.SINGLE_TAG_XML_PARSE_START);

        boolean canReadNextXmlTag = true; // To check if the given XML only has a single tag.
        ArrayList<String> attributeValues = new ArrayList<String>();

        try {
            // Instantiate pure Java XML parser.
            XmlPullParserFactory factory = XmlPullParserFactory.newInstance();
            XmlPullParser parser = factory.newPullParser();
            parser.setInput(new StringReader(xmlString));
            int eventType = parser.getEventType();

            while (eventType != XmlPullParser.END_DOCUMENT) {
                if (eventType != XmlPullParser.START_TAG) {
                    eventType = parser.next();
                    continue;
                }

                if (!canReadNextXmlTag) {
                    // More than one tag in the given XML. Therefore it is not a single tag XML.
                    GuidedBrowsingMetrics.recordParseSingleTagXmlActionEvent(
                            ParseSingleTagXmlActionEvent.SINGLE_TAG_XML_PARSE_INCORRECT_DATA);
                    return new String[0];
                }

                for (String attribute : attributes) {
                    String attributeValue =
                            parser.getAttributeValue(/* namespace= */ null, attribute);
                    if (attributeValue == null) {
                        // Given attribute was not found in the XML.
                        GuidedBrowsingMetrics.recordParseSingleTagXmlActionEvent(
                                ParseSingleTagXmlActionEvent.SINGLE_TAG_XML_PARSE_SOME_KEY_MISSING);
                        return new String[0];
                    }

                    attributeValues.add(attributeValue);
                }
                // There has to be only one tag in the given XML.
                canReadNextXmlTag = false;
                eventType = parser.next();
            }
        } catch (XmlPullParserException | IOException e) {
            // Given input was not a XML or there were some issues in reading it.
            GuidedBrowsingMetrics.recordParseSingleTagXmlActionEvent(
                    ParseSingleTagXmlActionEvent.SINGLE_TAG_XML_PARSE_INCORRECT_DATA);
            return new String[0];
        }

        GuidedBrowsingMetrics.recordParseSingleTagXmlActionEvent(
                ParseSingleTagXmlActionEvent.SINGLE_TAG_XML_PARSE_SUCCESS);
        return attributeValues.toArray(new String[attributeValues.size()]);
    }
}
