// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.virtual_structure;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;

import android.app.assist.AssistStructure.ViewNode;
import android.graphics.Color;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.autofill.TestViewStructure;
import org.chromium.components.optimization_guide.content.PageContentProtoProviderBridge;
import org.chromium.components.optimization_guide.content.PageContentProtoProviderBridgeJni;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.AnnotatedPageContent;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.ContentAttributeType;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.ContentAttributes;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.ContentNode;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.FrameData;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.FrameInteractionInfo;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.Selection;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.TextInfo;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.TextSize;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.TextStyle;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link PageContentProtoViewStructureBuilder} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PageContentProtoViewStructureBuilderUnitTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PageContentProtoProviderBridge.Natives mMockNatives;
    @Mock private WebContents mWebContents;

    private static final String HISTOGRAM_NAME =
            "Android.PageContentProtoViewStructureBuilder.Result";

    @Before
    public void setUp() throws Exception {
        PageContentProtoProviderBridgeJni.setInstanceForTesting(mMockNatives);
    }

    @Test
    public void extractionErrorBuildsEmptyStructure() {
        PageContentProtoViewStructureBuilder builder = new PageContentProtoViewStructureBuilder();
        setEmptyNativePageContentResult();
        TestViewStructure structure = new TestViewStructure();
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(HISTOGRAM_NAME, false);

        builder.provideVirtualStructureForWebContents(structure, mWebContents);

        assertEquals(1, structure.getChildCount());
        var rootNode = structure.getChild(0);
        assertFalse(
                "Extras should not contain proto",
                rootNode.hasExtras()
                        && rootNode.getExtras()
                                .containsKey(
                                        PageContentProtoViewStructureBuilder.APC_PROTO_EXTRA_KEY));
        histogramWatcher.assertExpected();
    }

    @Test
    public void extractionGetsNodeText() {
        PageContentProtoViewStructureBuilder builder = new PageContentProtoViewStructureBuilder();
        String nodeText = "Node text";

        AnnotatedPageContent.Builder pageContentBuilder = AnnotatedPageContent.newBuilder();
        var rootNode = getRootContentNode(1);
        var textNode =
                getContentNodeWithText(
                        2,
                        nodeText,
                        TextSize.TEXT_SIZE_M_DEFAULT,
                        Color.GREEN,
                        /* hasEmphasis= */ true);
        rootNode.addChildrenNodes(textNode);
        pageContentBuilder.setRootNode(rootNode);
        setNativePageContentResult(pageContentBuilder.build());
        TestViewStructure structure = new TestViewStructure();
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(HISTOGRAM_NAME, true);

        builder.provideVirtualStructureForWebContents(structure, mWebContents);

        assertEquals(1, structure.getChildCount());
        var rootStructureNode = structure.getChild(0);
        var textStructureNode = rootStructureNode.getChild(0);
        assertEquals(nodeText, textStructureNode.getText());
        assertEquals(16, textStructureNode.getTextSize(), 0.001);
        assertEquals(Color.GREEN, textStructureNode.getTextFgColor());
        // hasEmphasis doesn't specify what kind of emphasis (bold, italic, underline), so we just
        // set TEXT_STYLE_BOLD.
        assertEquals(ViewNode.TEXT_STYLE_BOLD, textStructureNode.getTextStyleFlags());
        assertEquals("android.widget.TextView", textStructureNode.getClassName());
        histogramWatcher.assertExpected();
    }

    @Test
    public void extractionAddsSelectionInfo() {
        PageContentProtoViewStructureBuilder builder = new PageContentProtoViewStructureBuilder();
        AnnotatedPageContent.Builder pageContentBuilder = AnnotatedPageContent.newBuilder();
        var rootNode = getRootContentNode(1);
        var firstTextNode =
                getContentNodeWithText(
                        2, "Hello", TextSize.TEXT_SIZE_L, Color.GREEN, /* hasEmphasis= */ false);
        var secondTextNode =
                getContentNodeWithText(
                        3, ", ", TextSize.TEXT_SIZE_M_DEFAULT, Color.RED, /* hasEmphasis= */ false);
        var thirdTextNode =
                getContentNodeWithText(
                        4, "World!", TextSize.TEXT_SIZE_S, Color.BLUE, /* hasEmphasis= */ true);
        var fourthTextNode =
                getContentNodeWithText(
                        5, "Text.", TextSize.TEXT_SIZE_XS, Color.BLACK, /* hasEmphasis= */ false);
        // Selection starts at node 2, specifically at character index 2 "llo". It finishes at node
        // 4, specifically at character index 5 "World".
        // This means that nodes 2, 3 and 4 are selected.
        var frameDataWithSelection = getMainFrameDataWithSelection(2, 4, 2, 5);
        rootNode.addChildrenNodes(firstTextNode)
                .addChildrenNodes(secondTextNode)
                .addChildrenNodes(thirdTextNode)
                .addChildrenNodes(fourthTextNode);
        pageContentBuilder.setRootNode(rootNode).setMainFrameData(frameDataWithSelection);

        setNativePageContentResult(pageContentBuilder.build());
        TestViewStructure structure = new TestViewStructure();

        builder.provideVirtualStructureForWebContents(structure, mWebContents);

        var rootStructureNode = structure.getChild(0);
        var firstTextStructureNode = rootStructureNode.getChild(0);
        var secondTextStructureNode = rootStructureNode.getChild(1);
        var thirdTextStructureNode = rootStructureNode.getChild(2);
        var fourthTextStructureNode = rootStructureNode.getChild(3);

        assertIsSelected(2, firstTextStructureNode.getText().length(), firstTextStructureNode);
        assertIsSelected(0, secondTextStructureNode.getText().length(), secondTextStructureNode);
        assertIsSelected(0, 5, thirdTextStructureNode);
        assertFalse(fourthTextStructureNode.isSelected());
    }

    @Test
    public void extractionContainsProtoBytes() {
        PageContentProtoViewStructureBuilder builder = new PageContentProtoViewStructureBuilder();
        AnnotatedPageContent pageContent =
                AnnotatedPageContent.newBuilder().setRootNode(getRootContentNode(1)).build();
        setNativePageContentResult(pageContent);
        TestViewStructure structure = new TestViewStructure();

        builder.provideVirtualStructureForWebContents(structure, mWebContents);

        var rootStructureNode = structure.getChild(0);
        var extras = rootStructureNode.getExtras();
        assertTrue(extras.containsKey(PageContentProtoViewStructureBuilder.APC_PROTO_EXTRA_KEY));
        assertArrayEquals(
                pageContent.toByteArray(),
                extras.getByteArray(PageContentProtoViewStructureBuilder.APC_PROTO_EXTRA_KEY));
    }

    void assertIsSelected(
            int expectedStartOffset, int expectedEndOffset, TestViewStructure viewStructure) {
        assertTrue("View should be selected", viewStructure.isSelected());
        assertEquals(
                "View selection start offset should match",
                expectedStartOffset,
                viewStructure.getTextSelectionStart());
        assertEquals(
                "View selection end offset should match",
                expectedEndOffset,
                viewStructure.getTextSelectionEnd());
    }

    private FrameData.Builder getMainFrameDataWithSelection(
            int startNodeId, int endNodeId, int startOffset, int endOffset) {
        return FrameData.newBuilder()
                .setFrameInteractionInfo(
                        FrameInteractionInfo.newBuilder()
                                .setSelection(
                                        Selection.newBuilder()
                                                .setStartNodeId(startNodeId)
                                                .setEndNodeId(endNodeId)
                                                .setStartOffset(startOffset)
                                                .setEndOffset(endOffset)));
    }

    private ContentNode.Builder getContentNodeWithText(
            int nodeId, String text, TextSize size, int color, boolean hasEmphasis) {
        return ContentNode.newBuilder()
                .setContentAttributes(
                        ContentAttributes.newBuilder()
                                .setCommonAncestorDomNodeId(nodeId)
                                .setAttributeType(ContentAttributeType.CONTENT_ATTRIBUTE_TEXT)
                                .setTextData(
                                        TextInfo.newBuilder()
                                                .setTextContent(text)
                                                .setTextStyle(
                                                        TextStyle.newBuilder()
                                                                .setColor(color)
                                                                .setHasEmphasis(hasEmphasis)
                                                                .setTextSize(size))));
    }

    private ContentNode.Builder getRootContentNode(int rootNodeId) {
        return ContentNode.newBuilder()
                .setContentAttributes(
                        ContentAttributes.newBuilder()
                                .setCommonAncestorDomNodeId(rootNodeId)
                                .setAttributeType(ContentAttributeType.CONTENT_ATTRIBUTE_ROOT));
    }

    private void setNativePageContentResult(AnnotatedPageContent pageContentResult) {
        doAnswer(
                        invocationOnMock -> {
                            Callback<byte[]> nativeCallback = invocationOnMock.getArgument(1);
                            nativeCallback.onResult(pageContentResult.toByteArray());
                            return null;
                        })
                .when(mMockNatives)
                .getAiPageContent(eq(mWebContents), any());
    }

    private void setEmptyNativePageContentResult() {
        doAnswer(
                        invocationOnMock -> {
                            Callback<byte[]> nativeCallback = invocationOnMock.getArgument(1);
                            nativeCallback.onResult(new byte[] {});
                            return null;
                        })
                .when(mMockNatives)
                .getAiPageContent(eq(mWebContents), any());
    }
}
