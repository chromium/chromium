// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.virtual_structure;

import android.app.assist.AssistStructure.ViewNode;
import android.os.Bundle;
import android.view.ViewStructure;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.optimization_guide.content.PageContentProtoProviderBridge;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.AnnotatedPageContent;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.ContentAttributeType;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.ContentAttributes;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.ContentNode;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.FrameData;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.IframeData;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.ImageInfo;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.Selection;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.TextInfo;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.TextSize;
import org.chromium.content_public.browser.WebContents;

@NullMarked
public class PageContentProtoViewStructureBuilder implements VirtualStructureProvider {

    @VisibleForTesting
    static final String APC_PROTO_EXTRA_KEY = "org.chromium.chrome.browser.AnnotatedPageContents";

    // Pixels in 1 em, returned for text labeled as TEXT_SIZE_M_DEFAULT.
    private static final float PIXELS_PER_EM = 16;
    // Pixels in 2 em, returned for text labeled as TEXT_SIZE_XL.
    private static final float XL_TEXT_SIZE = PIXELS_PER_EM * 2;
    // Pixels in 1.16 em, returned for text labeled as TEXT_SIZE_L.
    private static final float L_TEXT_SIZE = PIXELS_PER_EM * 1.16f;
    // Pixels in 0.83 em, returned for text labeled as TEXT_SIZE_S.
    private static final float S_TEXT_SIZE = PIXELS_PER_EM * 0.83f;
    // Pixels in 0.67 em, returned for text labeled as TEXT_SIZE_XS.
    private static final float XS_TEXT_SIZE = PIXELS_PER_EM * 0.67f;

    private static String getClassNameForAttributeType(ContentAttributeType attributeType) {
        return switch (attributeType) {
            case CONTENT_ATTRIBUTE_ORDERED_LIST, CONTENT_ATTRIBUTE_UNORDERED_LIST ->
                    "android.widget.ListView";
            case CONTENT_ATTRIBUTE_CONTAINER -> "android.view.View";
            case CONTENT_ATTRIBUTE_TEXT, CONTENT_ATTRIBUTE_PARAGRAPH -> "android.widget.TextView";
            case CONTENT_ATTRIBUTE_IMAGE -> "android.widget.ImageView";
            default -> "";
        };
    }

    private static String getTagNameForAttributeType(ContentAttributeType attributeType) {
        return switch (attributeType) {
            case CONTENT_ATTRIBUTE_UNKNOWN -> "UNKNOWN";
            case CONTENT_ATTRIBUTE_ROOT -> "HTML";
            case CONTENT_ATTRIBUTE_CONTAINER -> "DIV";
            case CONTENT_ATTRIBUTE_IFRAME -> "IFRAME";
            case CONTENT_ATTRIBUTE_PARAGRAPH -> "P";
            case CONTENT_ATTRIBUTE_HEADING -> "H";
            case CONTENT_ATTRIBUTE_TEXT -> "TEXTAREA";
            case CONTENT_ATTRIBUTE_ANCHOR -> "A";
            case CONTENT_ATTRIBUTE_IMAGE -> "IMG";
            case CONTENT_ATTRIBUTE_SVG_ROOT -> "SVG";
            case CONTENT_ATTRIBUTE_CANVAS -> "CANVAS";
            case CONTENT_ATTRIBUTE_VIDEO -> "VIDEO";
            case CONTENT_ATTRIBUTE_ORDERED_LIST -> "OL";
            case CONTENT_ATTRIBUTE_UNORDERED_LIST -> "UL";
            case CONTENT_ATTRIBUTE_LIST_ITEM -> "LI";
            case CONTENT_ATTRIBUTE_FORM -> "FORM";
            case CONTENT_ATTRIBUTE_FORM_CONTROL -> "INPUT";
            case CONTENT_ATTRIBUTE_TABLE -> "TABLE";
            case CONTENT_ATTRIBUTE_TABLE_ROW -> "TR";
            case CONTENT_ATTRIBUTE_TABLE_CELL -> "TD";
            default -> "";
        };
    }

    /*
     * Converts a text size category (e.g. S, M, L, XL) into an approximate pixel size. Used because
     * the page content proto doesn't contain exact pixel sizes for text.
     */
    private static float getApproximatePixelSizeForTextSize(TextSize textSize) {
        return switch (textSize) {
            case TEXT_SIZE_XL -> XL_TEXT_SIZE;
            case TEXT_SIZE_L -> L_TEXT_SIZE;
            case TEXT_SIZE_M_DEFAULT -> PIXELS_PER_EM;
            case TEXT_SIZE_S -> S_TEXT_SIZE;
            case TEXT_SIZE_XS -> XS_TEXT_SIZE;
            default -> PIXELS_PER_EM;
        };
    }

    @Override
    public void provideVirtualStructureForWebContents(
            final ViewStructure structure, WebContents webContents) {

        if (webContents.isIncognito()) {
            structure.setChildCount(0);
            return;
        }
        structure.setChildCount(1);
        final ViewStructure rootNode = structure.asyncNewChild(0);

        PageContentProtoProviderBridge.getAiPageContent(
                webContents,
                result -> {
                    RecordHistogram.recordBooleanHistogram(
                            "Android.PageContentProtoViewStructureBuilder.Result", result != null);

                    if (result == null) {
                        rootNode.asyncCommit();
                        return;
                    }
                    populateVirtualStructureWithPageContentProto(rootNode, result);
                });
    }

    private static void populateVirtualStructureWithPageContentProto(
            final ViewStructure viewRoot, AnnotatedPageContent annotatedPageContent) {
        Selection selectionInfo = null;
        if (annotatedPageContent.hasMainFrameData()) {
            var mainFrameData = annotatedPageContent.getMainFrameData();
            selectionInfo = getSelectionInfo(mainFrameData);
        }

        Bundle extras = viewRoot.getExtras();
        extras.putByteArray(APC_PROTO_EXTRA_KEY, annotatedPageContent.toByteArray());

        populateNode(viewRoot, annotatedPageContent.getRootNode(), selectionInfo);
        viewRoot.asyncCommit();
    }

    private static @Nullable Selection getSelectionInfo(FrameData frameData) {
        if (frameData.hasFrameInteractionInfo()) {
            var frameInteractionInfo = frameData.getFrameInteractionInfo();
            if (frameInteractionInfo.hasSelection()) {
                return frameInteractionInfo.getSelection();
            }
        }
        return null;
    }

    private static void populateNode(
            final ViewStructure node, ContentNode protoNode, @Nullable Selection selectionInfo) {
        node.setChildCount(protoNode.getChildrenNodesCount());

        if (protoNode.hasContentAttributes()) {
            ContentAttributes contentAttributes = protoNode.getContentAttributes();

            int textNodeSelectionStartOffset = 0;
            int textNodeSelectionEndOffset = 0;
            boolean hasSelection = false;
            if (selectionInfo != null && contentAttributes.hasCommonAncestorDomNodeId()) {
                var domNodeId = contentAttributes.getCommonAncestorDomNodeId();
                if (domNodeId >= selectionInfo.getStartNodeId()
                        && domNodeId <= selectionInfo.getEndNodeId()) {
                    node.setSelected(true);
                    hasSelection = true;
                    if (domNodeId == selectionInfo.getStartNodeId()) {
                        textNodeSelectionStartOffset = selectionInfo.getStartOffset();
                    }

                    if (domNodeId == selectionInfo.getEndNodeId()) {
                        textNodeSelectionEndOffset = selectionInfo.getEndOffset();
                    } else if (contentAttributes.hasTextData()) {
                        textNodeSelectionEndOffset =
                                contentAttributes.getTextData().getTextContent().length();
                    }
                }
            }

            if (contentAttributes.hasAttributeType()) {
                var attributeType = contentAttributes.getAttributeType();
                var className = getClassNameForAttributeType(attributeType);
                var tagName = getTagNameForAttributeType(attributeType);
                var htmlInfoBuilder = node.newHtmlInfoBuilder(tagName);
                node.setHtmlInfo(htmlInfoBuilder.build());
                node.setClassName(className);
            }

            if (contentAttributes.hasTextData()) {
                TextInfo textInfo = contentAttributes.getTextData();
                if (hasSelection) {
                    node.setText(
                            textInfo.getTextContent(),
                            textNodeSelectionStartOffset,
                            textNodeSelectionEndOffset);
                } else {
                    node.setText(textInfo.getTextContent());
                }
                if (textInfo.hasTextStyle()) {
                    var textStyle = textInfo.getTextStyle();
                    var textPixelSize = getApproximatePixelSizeForTextSize(textStyle.getTextSize());
                    node.setTextStyle(
                            textPixelSize,
                            /* fgColor= */ textStyle.getColor(),
                            /* bgColor= */ ViewNode.TEXT_COLOR_UNDEFINED,
                            textStyle.getHasEmphasis() ? ViewNode.TEXT_STYLE_BOLD : 0);
                }
            } else if (contentAttributes.hasImageData()) {
                ImageInfo imageData = contentAttributes.getImageData();
                if (imageData.hasImageCaption()) {
                    // Set image caption with 'setText' to match behavior of ViewStructureBuilder.
                    node.setText(imageData.getImageCaption());
                }
            } else if (contentAttributes.hasIframeData()) {
                IframeData iframeData = contentAttributes.getIframeData();
                if (iframeData.getFrameData().getFrameInteractionInfo().hasSelection()) {
                    // If an iframe has its own selection then use it on its children.
                    selectionInfo =
                            iframeData.getFrameData().getFrameInteractionInfo().getSelection();
                } else {
                    // Ignore outer selection info inside iframe.
                    selectionInfo = null;
                }
            }
        }

        int childNodeIndex = 0;
        for (ContentNode childNode : protoNode.getChildrenNodesList()) {
            var newNode = node.asyncNewChild(childNodeIndex);
            populateNode(newNode, childNode, selectionInfo);
            newNode.asyncCommit();
            childNodeIndex++;
        }
    }
}
