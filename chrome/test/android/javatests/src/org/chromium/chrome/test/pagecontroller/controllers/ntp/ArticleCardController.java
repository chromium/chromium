// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.ntp;

import static org.chromium.chrome.test.pagecontroller.utils.Ui2Locators.withText;
import static org.chromium.chrome.test.pagecontroller.utils.Ui2Locators.withTextRegex;

import android.support.test.uiautomator.UiObject2;

import com.google.common.base.Joiner;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.ElementController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;
import org.chromium.chrome.test.pagecontroller.utils.UiLocationException;
import org.chromium.chrome.test.pagecontroller.utils.UiLocatorHelper;

import java.util.List;
import java.util.Objects;

/**
 * NTP Article Element Controller.
 */
public class ArticleCardController extends ElementController {
    // Implementation type of the article card.
    public enum ImplementationType { ZINE, FEED };

    /**
     * Represents a single article, can be used by the NewTabPageController
     * to perform actions.
     */
    public static class Info {
        private final String mHeadline, mPublisher, mAge;
        private final ImplementationType mImplType;

        public Info(String headline, String publisher, String age,
                ImplementationType implementationType) {
            mHeadline = headline;
            mPublisher = publisher;
            mAge = age;
            mImplType = implementationType;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof Info)) return false;
            Info that = (Info) o;
            return Objects.equals(mHeadline, that.mHeadline)
                    && Objects.equals(mPublisher, that.mPublisher)
                    && Objects.equals(mAge, that.mAge) && mImplType == that.mImplType;
        }

        @Override
        public int hashCode() {
            return Objects.hash(mHeadline, mPublisher, mAge, mImplType);
        }

        @Override
        public String toString() {
            return "ArticleCardController{"
                    + "mHeadline='" + mHeadline + '\'' + ", mPublisher='" + mPublisher + '\''
                    + ", mAge='" + mAge + '\'' + ", mImplementation=" + mImplType + '}';
        }

        public String getHeadline() {
            return mHeadline;
        }

        public String getPublisher() {
            return mPublisher;
        }

        public String getAge() {
            return mAge;
        }

        public ImplementationType getImplementationType() {
            return mImplType;
        }
    }

    private abstract static class ArticleImpl {
        public abstract List<Info> parseScreenForArticles(UiLocatorHelper locatorHelper);
        public abstract IUi2Locator getLocator(Info cardInfo);
        protected List<Info> parseScreenForArticles(final UiLocatorHelper locatorHelper,
                final ImplementationType implementationType, final IUi2Locator cardsLocator,
                final IUi2Locator headlineLocator, final IUi2Locator publisherLocator,
                final IUi2Locator ageLocator) {
            return locatorHelper.getCustomElements(
                    cardsLocator, new UiLocatorHelper.CustomElementMaker<Info>() {
                        @Override
                        public Info makeElement(UiObject2 articleCard, boolean isLastAttempt) {
                            String headline =
                                    locatorHelper.getOneTextImmediate(headlineLocator, articleCard);
                            String publisher = locatorHelper.getOneTextImmediate(
                                    publisherLocator, articleCard);
                            String age = locatorHelper.getOneTextImmediate(ageLocator, articleCard);
                            if (headline != null && publisher != null && age != null) {
                                return new Info(headline, publisher, age, implementationType);
                            } else if (isLastAttempt) {
                                return null;
                            } else {
                                if (headline == null) {
                                    throw new UiLocationException(
                                            "Headline not found.", headlineLocator, articleCard);
                                } else if (publisher == null) {
                                    throw new UiLocationException(
                                            "Publisher not found.", publisherLocator, articleCard);
                                } else {
                                    throw new UiLocationException(
                                            "Age not found.", ageLocator, articleCard);
                                }
                            }
                        }
                    });
        }
    }

    private static class FeedArticleImpl extends ArticleImpl {
        private static final IUi2Locator LOCATOR_NON_EMPTY_STRING = withTextRegex(".+");
        private static final IUi2Locator LOCATOR_CARDS = Ui2Locators.withPath(
                Ui2Locators.withAnyResEntry(R.id.content),
                Ui2Locators.withAnyResEntry(com.google.android.libraries.feed.basicstream.R.id
                                                    .feed_stream_recycler_view),
                Ui2Locators.withAnyResEntry(com.google.android.libraries.feed.basicstream.internal
                                                    .viewholders.R.id.feed_content_card));
        private static final IUi2Locator LOCATOR_HEADLINE =
                Ui2Locators.withPath(Ui2Locators.withChildIndex(0, 6), LOCATOR_NON_EMPTY_STRING);
        private static final IUi2Locator LOCATOR_PUBLISHER =
                Ui2Locators.withPath(Ui2Locators.withChildIndex(0, 5),
                        Ui2Locators.withChildIndex(1, 2), LOCATOR_NON_EMPTY_STRING);
        private static final IUi2Locator LOCATOR_AGE = Ui2Locators.withPath(
                Ui2Locators.withChildIndex(0, 5), Ui2Locators.withChildIndex(1),
                Ui2Locators.withChildIndex(2), LOCATOR_NON_EMPTY_STRING);
        @Override
        public List<Info> parseScreenForArticles(UiLocatorHelper locatorHelper) {
            return parseScreenForArticles(locatorHelper, ImplementationType.FEED, LOCATOR_CARDS,
                    LOCATOR_HEADLINE, LOCATOR_PUBLISHER, LOCATOR_AGE);
        }
        @Override
        public IUi2Locator getLocator(Info cardInfo) {
            return Ui2Locators.withPath(LOCATOR_HEADLINE, withText(cardInfo.getHeadline()));
        }
    }

    private static class ZineArticleImpl extends ArticleImpl {
        private static final IUi2Locator LOCATOR_CARDS =
                Ui2Locators.withPath(Ui2Locators.withAnyResEntry(R.id.content),
                        Ui2Locators.withAnyResEntry(R.id.card_contents));
        private static final IUi2Locator LOCATOR_HEADLINE =
                Ui2Locators.withAnyResEntry(R.id.article_headline);
        private static final IUi2Locator LOCATOR_PUBLISHER =
                Ui2Locators.withAnyResEntry(R.id.article_publisher);
        private static final IUi2Locator LOCATOR_AGE =
                Ui2Locators.withAnyResEntry(R.id.article_age);
        @Override
        public List<Info> parseScreenForArticles(UiLocatorHelper locatorHelper) {
            return parseScreenForArticles(locatorHelper, ImplementationType.ZINE, LOCATOR_CARDS,
                    LOCATOR_HEADLINE, LOCATOR_PUBLISHER, LOCATOR_AGE);
        }
        @Override
        public IUi2Locator getLocator(Info cardInfo) {
            return Ui2Locators.withPath(LOCATOR_HEADLINE, withText(cardInfo.getHeadline()));
        }
    }

    private static ArticleCardController sInstance = new ArticleCardController();
    private ArticleImpl mFeedImpl, mZineImpl;
    private ArticleCardController() {
        mFeedImpl = new FeedArticleImpl();
        mZineImpl = new ZineArticleImpl();
    }

    public static ArticleCardController getInstance() {
        return sInstance;
    }

    public IUi2Locator getLocator(Info cardInfo) {
        return getCurrentImplementation(cardInfo.getImplementationType()).getLocator(cardInfo);
    }

    public List<Info> parseScreenForArticles(final ImplementationType cardImplementationType) {
        return getCurrentImplementation(cardImplementationType)
                .parseScreenForArticles(mLocatorHelper);
    }

    public String articlesToString(List<Info> cards) {
        return Joiner.on("\n").join(cards);
    }

    private ArticleImpl getCurrentImplementation(ImplementationType implementationType) {
        switch (implementationType) {
            case FEED:
                return mFeedImpl;
            case ZINE:
                return mZineImpl;
        }
        throw new IllegalArgumentException("Unknown implementation type" + implementationType);
    }
}
