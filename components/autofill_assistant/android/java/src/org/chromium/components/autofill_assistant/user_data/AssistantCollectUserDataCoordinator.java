// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.user_data;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.components.autofill_assistant.AssistantEditorFactory;
import org.chromium.components.autofill_assistant.AssistantTagsForTesting;
import org.chromium.components.autofill_assistant.LayoutUtils;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.autofill_assistant.user_data.additional_sections.AssistantAdditionalSectionContainer;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

// TODO(crbug.com/806868): Use mCarouselCoordinator to show chips.

/**
 * Coordinator for the Payment Request.
 */
public class AssistantCollectUserDataCoordinator {
    public static final String DIVIDER_TAG = "divider";
    private final Activity mActivity;
    private final LinearLayout mPaymentRequestUI;
    private final AssistantCollectUserDataModel mModel;
    private AssistantCollectUserDataBinder.ViewHolder mViewHolder;

    public AssistantCollectUserDataCoordinator(Activity activity,
            AssistantCollectUserDataModel model, AssistantEditorFactory editorFactory,
            WindowAndroid windowAndroid) {
        mActivity = activity;
        mModel = model;
        int sectionToSectionPadding = activity.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_payment_request_section_padding);

        mPaymentRequestUI = new LinearLayout(mActivity);
        mPaymentRequestUI.setOrientation(LinearLayout.VERTICAL);
        mPaymentRequestUI.setLayoutParams(
                new ViewGroup.LayoutParams(/* width= */ ViewGroup.LayoutParams.MATCH_PARENT,
                        /* height= */ ViewGroup.LayoutParams.WRAP_CONTENT));

        AssistantVerticalExpanderAccordion paymentRequestExpanderAccordion =
                new AssistantVerticalExpanderAccordion(mActivity, null);
        paymentRequestExpanderAccordion.setOrientation(LinearLayout.VERTICAL);
        paymentRequestExpanderAccordion.setLayoutParams(
                new LinearLayout.LayoutParams(/* width= */ ViewGroup.LayoutParams.MATCH_PARENT,
                        /* height= */ 0, /* weight= */ 1));
        paymentRequestExpanderAccordion.setOnExpandedViewChangedListener(
                expander -> mModel.set(AssistantCollectUserDataModel.EXPANDED_SECTION, expander));

        mPaymentRequestUI.addView(paymentRequestExpanderAccordion,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        AssistantAdditionalSectionContainer prependedSections =
                new AssistantAdditionalSectionContainer(mActivity, paymentRequestExpanderAccordion);

        LinearLayout genericUserInterfaceContainerPrepended = new LinearLayout(activity);
        genericUserInterfaceContainerPrepended.setOrientation(LinearLayout.VERTICAL);
        paymentRequestExpanderAccordion.addView(genericUserInterfaceContainerPrepended,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        AssistantLoginSection loginSection =
                new AssistantLoginSection(mActivity, paymentRequestExpanderAccordion);
        createSeparator(paymentRequestExpanderAccordion);
        AssistantContactDetailsSection contactDetailsSection =
                new AssistantContactDetailsSection(mActivity, paymentRequestExpanderAccordion);
        createSeparator(paymentRequestExpanderAccordion);
        AssistantPhoneNumberSection phoneNumberSection =
                new AssistantPhoneNumberSection(mActivity, paymentRequestExpanderAccordion);
        createSeparator(paymentRequestExpanderAccordion);
        AssistantPaymentMethodSection paymentMethodSection =
                new AssistantPaymentMethodSection(mActivity, paymentRequestExpanderAccordion);
        createSeparator(paymentRequestExpanderAccordion);
        AssistantShippingAddressSection shippingAddressSection =
                new AssistantShippingAddressSection(mActivity, paymentRequestExpanderAccordion);
        createSeparator(paymentRequestExpanderAccordion);

        AssistantAdditionalSectionContainer appendedSections =
                new AssistantAdditionalSectionContainer(mActivity, paymentRequestExpanderAccordion);

        LinearLayout genericUserInterfaceContainerAppended = new LinearLayout(activity);
        genericUserInterfaceContainerAppended.setOrientation(LinearLayout.VERTICAL);
        paymentRequestExpanderAccordion.addView(genericUserInterfaceContainerAppended,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        AssistantInfoSection infoSection =
                new AssistantInfoSection(mActivity, paymentRequestExpanderAccordion);

        AssistantTermsSection termsSection = new AssistantTermsSection(
                mActivity, paymentRequestExpanderAccordion, /* showAsSingleCheckbox= */ false);
        AssistantTermsSection termsAsCheckboxSection =
                new AssistantTermsSection(mActivity, paymentRequestExpanderAccordion,
                        /* showAsSingleCheckbox= */ true);

        AssistantDataOriginNotice dataOriginNotice = new AssistantDataOriginNotice(
                mActivity, windowAndroid, paymentRequestExpanderAccordion);

        paymentRequestExpanderAccordion.setTag(
                AssistantTagsForTesting.COLLECT_USER_DATA_ACCORDION_TAG);
        loginSection.getView().setTag(AssistantTagsForTesting.COLLECT_USER_DATA_LOGIN_SECTION_TAG);
        contactDetailsSection.getView().setTag(
                AssistantTagsForTesting.COLLECT_USER_DATA_CONTACT_DETAILS_SECTION_TAG);
        phoneNumberSection.getView().setTag(
                AssistantTagsForTesting.COLLECT_USER_DATA_PHONE_NUMBER_SECTION_TAG);
        paymentMethodSection.getView().setTag(
                AssistantTagsForTesting.COLLECT_USER_DATA_PAYMENT_METHOD_SECTION_TAG);
        shippingAddressSection.getView().setTag(
                AssistantTagsForTesting.COLLECT_USER_DATA_SHIPPING_ADDRESS_SECTION_TAG);
        termsSection.getView().setTag(
                AssistantTagsForTesting.COLLECT_USER_DATA_RADIO_TERMS_SECTION_TAG);
        termsAsCheckboxSection.getView().setTag(
                AssistantTagsForTesting.COLLECT_USER_DATA_CHECKBOX_TERMS_SECTION_TAG);
        infoSection.getView().setTag(AssistantTagsForTesting.COLLECT_USER_DATA_INFO_SECTION_TAG);
        dataOriginNotice.getView().setTag(
                AssistantTagsForTesting.COLLECT_USER_DATA_DATA_ORIGIN_NOTICE_TAG);

        // Bind view and mediator through the model.
        mViewHolder = new AssistantCollectUserDataBinder.ViewHolder(mPaymentRequestUI,
                paymentRequestExpanderAccordion, sectionToSectionPadding, loginSection,
                contactDetailsSection, phoneNumberSection, paymentMethodSection,
                shippingAddressSection, termsSection, termsAsCheckboxSection, infoSection,
                prependedSections, appendedSections, dataOriginNotice,
                genericUserInterfaceContainerPrepended, genericUserInterfaceContainerAppended,
                DIVIDER_TAG, mActivity, editorFactory, windowAndroid);
        AssistantCollectUserDataBinder binder = new AssistantCollectUserDataBinder();
        PropertyModelChangeProcessor.create(model, mViewHolder, binder);

        // View is initially invisible.
        model.set(AssistantCollectUserDataModel.VISIBLE, false);
    }

    public View getView() {
        return mPaymentRequestUI;
    }

    /**
     * Explicitly clean up.
     */
    public void destroy() {
        mViewHolder = null;
    }

    private void createSeparator(ViewGroup parent) {
        View divider = LayoutUtils.createInflater(mActivity).inflate(
                R.layout.autofill_assistant_payment_request_section_divider, parent, false);
        divider.setTag(DIVIDER_TAG);
        parent.addView(divider);
    }
}
