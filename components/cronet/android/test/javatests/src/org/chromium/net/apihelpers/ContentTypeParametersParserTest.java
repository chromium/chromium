// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.apihelpers;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;

import java.util.Map;

/** Unit tests for {@link ContentTypeParametersParser}. */
@RunWith(AndroidJUnit4.class)
@Batch(Batch.UNIT_TESTS)
public class ContentTypeParametersParserTest {
    @Test
    @SmallTest
    public void testSingleParam_simple() throws Exception {
        String header = "text/html;charset=utf-8";

        ContentTypeParametersParser parser = new ContentTypeParametersParser(header);
        Map.Entry<String, String> parameter = parser.getNextParameter();

        assertThat(parameter.getKey()).isEqualTo("charset");
        assertThat(parameter.getValue()).isEqualTo("utf-8");
        assertThat(parser.hasMore()).isFalse();
    }

    @Test
    @SmallTest
    public void testParser_quoted_noEscape() throws Exception {
        String header = "text/html;charset=\"utf-8\"";

        ContentTypeParametersParser parser = new ContentTypeParametersParser(header);
        Map.Entry<String, String> parameter = parser.getNextParameter();

        assertThat(parameter.getKey()).isEqualTo("charset");
        assertThat(parameter.getValue()).isEqualTo("utf-8");
        assertThat(parser.hasMore()).isFalse();
    }

    @Test
    @SmallTest
    public void testParser_quoted_noEscapeWithSpace() throws Exception {
        String header = "text/html;charset=\"utf-  8\"";

        ContentTypeParametersParser parser = new ContentTypeParametersParser(header);
        Map.Entry<String, String> parameter = parser.getNextParameter();

        assertThat(parameter.getKey()).isEqualTo("charset");
        assertThat(parameter.getValue()).isEqualTo("utf-  8");
        assertThat(parser.hasMore()).isFalse();
    }

    @Test
    @SmallTest
    public void testParser_quoted_escape() throws Exception {
        String header = "text/html;charset=\"utf-\\\\8\"";

        ContentTypeParametersParser parser = new ContentTypeParametersParser(header);
        Map.Entry<String, String> parameter = parser.getNextParameter();

        assertThat(parameter.getKey()).isEqualTo("charset");
        assertThat(parameter.getValue()).isEqualTo("utf-\\8");
        assertThat(parser.hasMore()).isFalse();
    }

    @Test
    @SmallTest
    public void testParser_multiple_mixed() throws Exception {
        String header = "text/html;charset=\"utf-\\\\8\";foo=\" bar\" ;   baz=quix ; abc=def";

        ContentTypeParametersParser parser = new ContentTypeParametersParser(header);

        Map.Entry<String, String> parameter = parser.getNextParameter();

        assertThat(parameter.getKey()).isEqualTo("charset");
        assertThat(parameter.getValue()).isEqualTo("utf-\\8");
        assertThat(parser.hasMore()).isTrue();

        parameter = parser.getNextParameter();

        assertThat(parameter.getKey()).isEqualTo("foo");
        assertThat(parameter.getValue()).isEqualTo(" bar");
        assertThat(parser.hasMore()).isTrue();

        parameter = parser.getNextParameter();

        assertThat(parameter.getKey()).isEqualTo("baz");
        assertThat(parameter.getValue()).isEqualTo("quix");
        assertThat(parser.hasMore()).isTrue();

        parameter = parser.getNextParameter();

        assertThat(parameter.getKey()).isEqualTo("abc");
        assertThat(parameter.getValue()).isEqualTo("def");
        assertThat(parser.hasMore()).isFalse();
    }

    @Test
    @SmallTest
    public void testParser_invalidTokenChar_key() throws Throwable {
        String header = "text/html;char\\set=utf8";

        ContentTypeParametersParser parser = new ContentTypeParametersParser(header);

        ContentTypeParametersParser.ContentTypeParametersParserException exception =
                assertThrows(
                        ContentTypeParametersParser.ContentTypeParametersParserException.class,
                        parser::getNextParameter);

        assertThat(exception.getErrorOffset()).isEqualTo(header.indexOf('\\'));
    }

    @Test
    @SmallTest
    public void testParser_invalidTokenChar_value() throws Throwable {
        String header = "text/html;charset=utf\\8";

        ContentTypeParametersParser parser = new ContentTypeParametersParser(header);

        ContentTypeParametersParser.ContentTypeParametersParserException exception =
                assertThrows(
                        ContentTypeParametersParser.ContentTypeParametersParserException.class,
                        parser::getNextParameter);

        assertThat(exception.getErrorOffset()).isEqualTo(header.indexOf('\\'));
    }

    @Test
    @SmallTest
    public void testParser_quotedStringNotClosed() throws Throwable {
        String header = "text/html;charset=\"utf-8";

        ContentTypeParametersParser parser = new ContentTypeParametersParser(header);

        ContentTypeParametersParser.ContentTypeParametersParserException exception =
                assertThrows(
                        ContentTypeParametersParser.ContentTypeParametersParserException.class,
                        parser::getNextParameter);

        assertThat(exception.getErrorOffset()).isEqualTo(header.indexOf('"'));
    }

    private <E extends Throwable> E assertThrows(Class<E> exceptionType, ThrowingRunnable runnable)
            throws Throwable {
        Throwable actualException = null;
        try {
            runnable.run();
        } catch (Throwable e) {
            actualException = e;
        }
        assertWithMessage("Exception not thrown").that(actualException).isNotNull();
        assertThat(actualException.getClass()).isEqualTo(exceptionType);
        return (E) actualException;
    }

    private interface ThrowingRunnable {
        public void run() throws Throwable;
    }
}
