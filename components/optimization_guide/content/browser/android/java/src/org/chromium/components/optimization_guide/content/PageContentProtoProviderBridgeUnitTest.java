// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.optimization_guide.content;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;

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
import org.chromium.build.annotations.Nullable;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.AnnotatedPageContent;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.ContentAttributes;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.ContentNode;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.TextInfo;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link PageContentProtoProviderBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PageContentProtoProviderBridgeUnitTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PageContentProtoProviderBridge.Natives mMockNatives;
    @Mock private WebContents mWebContents;

    @Before
    public void setUp() throws Exception {
        PageContentProtoProviderBridgeJni.setInstanceForTesting(mMockNatives);
    }

    @Test
    public void testGetAiPageContent() {
        var expectedResult = getPageContentProtoWithTextNode();
        setNativePageContentResult(expectedResult);
        PageContentProtoProviderBridge.getAiPageContent(
                mWebContents,
                new Callback<@Nullable AnnotatedPageContent>() {
                    @Override
                    public void onResult(AnnotatedPageContent result) {
                        assertTrue(result != null);
                        assertContentEquals(expectedResult, result);
                    }
                });
    }

    @Test
    public void testGetAiPageContent_emptyResult() {
        setEmptyNativePageContentResult();
        PageContentProtoProviderBridge.getAiPageContent(
                mWebContents,
                new Callback<@Nullable AnnotatedPageContent>() {
                    @Override
                    public void onResult(AnnotatedPageContent result) {
                        assertTrue(result == null);
                    }
                });
    }

    private AnnotatedPageContent getPageContentProtoWithTextNode() {
        return AnnotatedPageContent.newBuilder()
                .setRootNode(
                        ContentNode.newBuilder()
                                .setContentAttributes(
                                        ContentAttributes.newBuilder()
                                                .setCommonAncestorDomNodeId(123)
                                                .setTextData(
                                                        TextInfo.newBuilder()
                                                                .setTextContent("Testing!"))))
                .build();
    }

    private void assertContentEquals(AnnotatedPageContent expected, AnnotatedPageContent actual) {
        assertNotNull(actual);
        assertNotSame(expected, actual);
        assertEquals(expected.hasRootNode(), actual.hasRootNode());
        var expectedRootNode = expected.getRootNode();
        var actualRootNode = actual.getRootNode();
        assertEquals(
                expectedRootNode.hasContentAttributes(), actualRootNode.hasContentAttributes());
        var expectedContentAttributes = expectedRootNode.getContentAttributes();
        var actualContentAttributes = actualRootNode.getContentAttributes();
        assertEquals(
                expectedContentAttributes.getCommonAncestorDomNodeId(),
                actualContentAttributes.getCommonAncestorDomNodeId());
        assertEquals(
                expectedContentAttributes.hasTextData(), actualContentAttributes.hasTextData());
        var expectedTextData = expectedContentAttributes.getTextData();
        var actualTextData = actualContentAttributes.getTextData();
        assertEquals(expectedTextData.getTextContent(), actualTextData.getTextContent());
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
