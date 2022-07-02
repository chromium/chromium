// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.user_data;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.task.PostTask;
import org.chromium.components.autofill_assistant.AssistantAddressEditorGms;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantAddressEditor;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantContactEditor;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantPaymentInstrumentEditor;
import org.chromium.components.autofill_assistant.AssistantEditorFactory;
import org.chromium.components.autofill_assistant.AssistantOptionModel.AddressModel;
import org.chromium.components.autofill_assistant.AssistantOptionModel.ContactModel;
import org.chromium.components.autofill_assistant.AssistantOptionModel.PaymentInstrumentModel;
import org.chromium.components.autofill_assistant.AssistantPaymentInstrumentEditorGms;
import org.chromium.components.autofill_assistant.generic_ui.AssistantValue;
import org.chromium.components.autofill_assistant.user_data.AssistantCollectUserDataModel.LoginChoiceModel;
import org.chromium.components.autofill_assistant.user_data.additional_sections.AssistantAdditionalSection.Delegate;
import org.chromium.components.autofill_assistant.user_data.additional_sections.AssistantAdditionalSectionContainer;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * This class is responsible for pushing updates to the Autofill Assistant UI for requesting user
 * data. These updates are pulled from the {@link AssistantCollectUserDataModel} when a notification
 * of an update is received.
 */
class AssistantCollectUserDataBinder
        implements PropertyModelChangeProcessor.ViewBinder<AssistantCollectUserDataModel,
                AssistantCollectUserDataBinder.ViewHolder, PropertyKey> {
    /**
     * A wrapper class that holds the different views of the CollectUserData request.
     */
    static class ViewHolder {
        private final View mRootView;
        private final AssistantVerticalExpanderAccordion mPaymentRequestExpanderAccordion;
        private final int mSectionToSectionPadding;
        private final AssistantLoginSection mLoginSection;
        private final AssistantContactDetailsSection mContactDetailsSection;
        private final AssistantPhoneNumberSection mPhoneNumberSection;
        private final AssistantPaymentMethodSection mPaymentMethodSection;
        private final AssistantShippingAddressSection mShippingAddressSection;
        private final AssistantTermsSection mTermsSection;
        private final AssistantTermsSection mTermsAsCheckboxSection;
        private final AssistantInfoSection mInfoSection;
        private final AssistantAdditionalSectionContainer mPrependedSections;
        private final AssistantAdditionalSectionContainer mAppendedSections;
        private final AssistantDataOriginNotice mDataOriginNotice;
        private final ViewGroup mGenericUserInterfaceContainerPrepended;
        private final ViewGroup mGenericUserInterfaceContainerAppended;
        private final Object mDividerTag;
        private final Activity mActivity;
        @Nullable
        private final AssistantEditorFactory mEditorFactory;
        private final WindowAndroid mWindowAndroid;

        public ViewHolder(View rootView, AssistantVerticalExpanderAccordion accordion,
                int sectionPadding, AssistantLoginSection loginSection,
                AssistantContactDetailsSection contactDetailsSection,
                AssistantPhoneNumberSection phoneNumberSection,
                AssistantPaymentMethodSection paymentMethodSection,
                AssistantShippingAddressSection shippingAddressSection,
                AssistantTermsSection termsSection, AssistantTermsSection termsAsCheckboxSection,
                AssistantInfoSection infoSection,
                AssistantAdditionalSectionContainer prependedSections,
                AssistantAdditionalSectionContainer appendedSections,
                AssistantDataOriginNotice dataOriginNotice,
                ViewGroup genericUserInterfaceContainerPrepended,
                ViewGroup genericUserInterfaceContainerAppended, Object dividerTag,
                Activity activity, @Nullable AssistantEditorFactory editorFactory,
                WindowAndroid windowAndroid) {
            mRootView = rootView;
            mPaymentRequestExpanderAccordion = accordion;
            mSectionToSectionPadding = sectionPadding;
            mLoginSection = loginSection;
            mContactDetailsSection = contactDetailsSection;
            mPhoneNumberSection = phoneNumberSection;
            mPaymentMethodSection = paymentMethodSection;
            mShippingAddressSection = shippingAddressSection;
            mTermsSection = termsSection;
            mTermsAsCheckboxSection = termsAsCheckboxSection;
            mInfoSection = infoSection;
            mPrependedSections = prependedSections;
            mAppendedSections = appendedSections;
            mGenericUserInterfaceContainerPrepended = genericUserInterfaceContainerPrepended;
            mGenericUserInterfaceContainerAppended = genericUserInterfaceContainerAppended;
            mDataOriginNotice = dataOriginNotice;
            mDividerTag = dividerTag;
            mActivity = activity;
            mEditorFactory = editorFactory;
            mWindowAndroid = windowAndroid;
        }
    }

    @Override
    public void bind(
            AssistantCollectUserDataModel model, ViewHolder view, PropertyKey propertyKey) {
        boolean handled = updateEditors(model, propertyKey, view);
        handled = updateRootVisibility(model, propertyKey, view) || handled;
        handled = updateSectionTitles(model, propertyKey, view) || handled;
        handled = updateSectionContents(model, propertyKey, view) || handled;
        handled = updateSectionSelectedItem(model, propertyKey, view) || handled;
        handled = updateUiState(model, propertyKey, view) || handled;
        // Update section visibility/padding *after* updating editors and content.
        handled = updateVisibilityAndPaddings(model, propertyKey, view) || handled;

        if (propertyKey == AssistantCollectUserDataModel.DELEGATE) {
            AssistantCollectUserDataDelegate collectUserDataDelegate =
                    model.get(AssistantCollectUserDataModel.DELEGATE);

            AssistantTermsSection.Delegate termsDelegate =
                    collectUserDataDelegate == null ? null : new AssistantTermsSection.Delegate() {
                        @Override
                        public void onStateChanged(@AssistantTermsAndConditionsState int state) {
                            collectUserDataDelegate.onTermsAndConditionsChanged(state);
                        }

                        @Override
                        public void onLinkClicked(int link) {
                            // TODO(b/143128544) refactor to do this the right way.
                            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
                                view.mTermsSection.setTermsStatus(
                                        AssistantTermsAndConditionsState.NOT_SELECTED);
                                view.mTermsAsCheckboxSection.setTermsStatus(
                                        AssistantTermsAndConditionsState.NOT_SELECTED);
                            });
                            collectUserDataDelegate.onTextLinkClicked(link);
                        }
                    };
            view.mTermsSection.setDelegate(termsDelegate);
            view.mTermsAsCheckboxSection.setDelegate(termsDelegate);
            view.mInfoSection.setListener(collectUserDataDelegate != null
                            ? collectUserDataDelegate::onTextLinkClicked
                            : null);
            view.mContactDetailsSection.setDelegate(collectUserDataDelegate == null
                            ? null
                            : collectUserDataDelegate::onContactInfoChanged);
            view.mPhoneNumberSection.setDelegate(collectUserDataDelegate == null
                            ? null
                            : collectUserDataDelegate::onPhoneNumberChanged);
            view.mPaymentMethodSection.setDelegate(collectUserDataDelegate == null
                            ? null
                            : collectUserDataDelegate::onPaymentMethodChanged);
            view.mShippingAddressSection.setDelegate(collectUserDataDelegate == null
                            ? null
                            : collectUserDataDelegate::onShippingAddressChanged);
            view.mLoginSection.setDelegate(collectUserDataDelegate == null
                            ? null
                            : collectUserDataDelegate::onLoginChoiceChanged);
            view.mPrependedSections.setDelegate(collectUserDataDelegate != null
                            ? getAdditionalSectionsDelegate(collectUserDataDelegate)
                            : null);
            view.mAppendedSections.setDelegate(collectUserDataDelegate != null
                            ? getAdditionalSectionsDelegate(collectUserDataDelegate)
                            : null);
        } else {
            assert handled : "Unhandled property detected in AssistantCollectUserDataBinder!";
        }
    }

    private Delegate getAdditionalSectionsDelegate(
            AssistantCollectUserDataDelegate collectUserDataDelegate) {
        return new Delegate() {
            @Override
            public void onValueChanged(String key, AssistantValue value) {
                collectUserDataDelegate.onKeyValueChanged(key, value);
            }

            @Override
            public void onInputTextFocusChanged(boolean isFocused) {
                collectUserDataDelegate.onInputTextFocusChanged(isFocused);
            }
        };
    }

    private boolean shouldShowContactDetails(AssistantCollectUserDataModel model) {
        return model.get(AssistantCollectUserDataModel.REQUEST_NAME)
                || model.get(AssistantCollectUserDataModel.REQUEST_PHONE)
                || model.get(AssistantCollectUserDataModel.REQUEST_EMAIL);
    }

    private boolean shouldShowPhoneNumberSection(AssistantCollectUserDataModel model) {
        return model.get(AssistantCollectUserDataModel.REQUEST_PHONE_NUMBER_SEPARATELY);
    }

    private boolean shouldShowPaymentInstruments(AssistantCollectUserDataModel model) {
        return model.get(AssistantCollectUserDataModel.REQUEST_PAYMENT);
    }

    private boolean updateSectionTitles(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey == AssistantCollectUserDataModel.CONTACT_SECTION_TITLE) {
            view.mContactDetailsSection.setTitle(
                    model.get(AssistantCollectUserDataModel.CONTACT_SECTION_TITLE));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.PHONE_NUMBER_SECTION_TITLE) {
            view.mPhoneNumberSection.setTitle(
                    model.get(AssistantCollectUserDataModel.PHONE_NUMBER_SECTION_TITLE));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.LOGIN_SECTION_TITLE) {
            view.mLoginSection.setTitle(
                    model.get(AssistantCollectUserDataModel.LOGIN_SECTION_TITLE));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SHIPPING_SECTION_TITLE) {
            view.mShippingAddressSection.setTitle(
                    model.get(AssistantCollectUserDataModel.SHIPPING_SECTION_TITLE));
            return true;
        }
        return false;
    }

    /**
     * Updates the available items for each PR section.
     * @return whether the property key was handled.
     */
    private boolean updateSectionContents(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS) {
            if (shouldShowPaymentInstruments(model)) {
                view.mPaymentMethodSection.onAvailablePaymentMethodsChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_CONTACTS) {
            if (shouldShowContactDetails(model)) {
                view.mContactDetailsSection.onContactsChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_CONTACTS));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_PHONE_NUMBERS) {
            if (shouldShowPhoneNumberSection(model)) {
                view.mPhoneNumberSection.onPhoneNumbersChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_PHONE_NUMBERS));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_SHIPPING_ADDRESSES) {
            if (model.get(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS)) {
                view.mShippingAddressSection.onAddressesChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_SHIPPING_ADDRESSES));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_BILLING_ADDRESSES) {
            if (shouldShowPaymentInstruments(model)) {
                view.mPaymentMethodSection.onAddressesChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_BILLING_ADDRESSES));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_LOGINS) {
            if (model.get(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE)) {
                List<AssistantLoginChoice> loginChoices =
                        model.get(AssistantCollectUserDataModel.AVAILABLE_LOGINS);
                if (loginChoices != null) {
                    List<LoginChoiceModel> loginChoiceModels = new ArrayList<>();
                    for (AssistantLoginChoice loginChoice : loginChoices) {
                        loginChoiceModels.add(new LoginChoiceModel(loginChoice));
                    }
                    view.mLoginSection.onLoginsChanged(loginChoiceModels);
                }
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.PREPENDED_SECTIONS) {
            view.mPrependedSections.setSections(
                    model.get(AssistantCollectUserDataModel.PREPENDED_SECTIONS));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.APPENDED_SECTIONS) {
            view.mAppendedSections.setSections(
                    model.get(AssistantCollectUserDataModel.APPENDED_SECTIONS));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT) {
            view.mTermsSection.setAcceptTermsAndConditionsText(
                    model.get(AssistantCollectUserDataModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT));
            view.mTermsAsCheckboxSection.setAcceptTermsAndConditionsText(
                    model.get(AssistantCollectUserDataModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.TERMS_REQUIRE_REVIEW_TEXT) {
            view.mTermsSection.setTermsRequireReviewText(
                    model.get(AssistantCollectUserDataModel.TERMS_REQUIRE_REVIEW_TEXT));
            view.mTermsAsCheckboxSection.setTermsRequireReviewText(
                    model.get(AssistantCollectUserDataModel.TERMS_REQUIRE_REVIEW_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.INFO_SECTION_TEXT) {
            view.mInfoSection.setText(model.get(AssistantCollectUserDataModel.INFO_SECTION_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.INFO_SECTION_TEXT_CENTER) {
            view.mInfoSection.setCenter(
                    model.get(AssistantCollectUserDataModel.INFO_SECTION_TEXT_CENTER));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.PRIVACY_NOTICE_TEXT) {
            view.mTermsSection.setPrivacyNoticeText(
                    model.get(AssistantCollectUserDataModel.PRIVACY_NOTICE_TEXT));
            view.mTermsAsCheckboxSection.setPrivacyNoticeText(
                    model.get(AssistantCollectUserDataModel.PRIVACY_NOTICE_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_PREPENDED) {
            view.mGenericUserInterfaceContainerPrepended.removeAllViews();
            if (model.get(AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_PREPENDED) != null) {
                view.mGenericUserInterfaceContainerPrepended.addView(
                        model.get(AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_PREPENDED));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_APPENDED) {
            view.mGenericUserInterfaceContainerAppended.removeAllViews();
            if (model.get(AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_APPENDED) != null) {
                view.mGenericUserInterfaceContainerAppended.addView(
                        model.get(AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_APPENDED));
            }
            return true;
        } else if (propertyKey
                == AssistantCollectUserDataModel.CONTACT_SUMMARY_DESCRIPTION_OPTIONS) {
            view.mContactDetailsSection.setContactSummaryOptions(
                    model.get(AssistantCollectUserDataModel.CONTACT_SUMMARY_DESCRIPTION_OPTIONS));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.CONTACT_FULL_DESCRIPTION_OPTIONS) {
            view.mContactDetailsSection.setContactFullOptions(
                    model.get(AssistantCollectUserDataModel.CONTACT_FULL_DESCRIPTION_OPTIONS));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATA_ORIGIN_NOTICE_CONFIGURATION) {
            AssistantCollectUserDataModel.DataOriginNoticeConfiguration configuration =
                    model.get(AssistantCollectUserDataModel.DATA_ORIGIN_NOTICE_CONFIGURATION);
            view.mDataOriginNotice.setLinkText(configuration.getLinkText());
            view.mDataOriginNotice.setDialogTitle(configuration.getTitle());
            view.mDataOriginNotice.setDialogText(configuration.getText());
            view.mDataOriginNotice.setDialogButtonText(configuration.getButtonText());
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.ACCOUNT_EMAIL) {
            view.mDataOriginNotice.setAccountEmail(
                    model.get(AssistantCollectUserDataModel.ACCOUNT_EMAIL));
            return true;
        } else if (model.get(AssistantCollectUserDataModel.USE_ALTERNATIVE_EDIT_DIALOGS)) {
            view.mTermsAsCheckboxSection.useBackgroundlessPrivacyNotice();
            view.mTermsSection.useBackgroundlessPrivacyNotice();
            return true;
        }

        return false;
    }

    /**
     * Updates visibility of the root widget.
     * @return whether the property key was handled.
     */
    private boolean updateRootVisibility(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey != AssistantCollectUserDataModel.VISIBLE) {
            return false;
        }
        int visibility =
                model.get(AssistantCollectUserDataModel.VISIBLE) ? View.VISIBLE : View.GONE;
        if (view.mRootView.getVisibility() != visibility) {
            view.mRootView.setVisibility(visibility);
        }
        return true;
    }

    /**
     * Updates the currently selected item in each PR section.
     * @return whether the property key was handled.
     */
    private boolean updateSectionSelectedItem(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        // These changes are sent by the controller, do not notify it when selecting the added item.
        // This prevents creating a loop.
        if (propertyKey == AssistantCollectUserDataModel.SELECTED_SHIPPING_ADDRESS) {
            if (!model.get(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS)) {
                return true;
            }
            AddressModel shippingAddress =
                    model.get(AssistantCollectUserDataModel.SELECTED_SHIPPING_ADDRESS);
            if (shippingAddress != null) {
                view.mShippingAddressSection.addOrUpdateItem(
                        shippingAddress, /* select= */ true, /* notify= */ false);
            }
            // No need to reset selection if null, this will be handled by setItems().
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SELECTED_PAYMENT_INSTRUMENT) {
            if (!shouldShowPaymentInstruments(model)) {
                return true;
            }
            PaymentInstrumentModel paymentInstrument =
                    model.get(AssistantCollectUserDataModel.SELECTED_PAYMENT_INSTRUMENT);
            if (paymentInstrument != null) {
                view.mPaymentMethodSection.addOrUpdateItem(
                        paymentInstrument, /* select= */ true, /* notify= */ false);
            }
            // No need to reset selection if null, this will be handled by setItems().
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS) {
            if (!shouldShowContactDetails(model)) {
                return true;
            }
            ContactModel contact =
                    model.get(AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS);
            if (contact != null) {
                view.mContactDetailsSection.addOrUpdateItem(
                        contact, /* select= */ true, /* notify= */ false);
            }
            // No need to reset selection if null, this will be handled by setItems().
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SELECTED_PHONE_NUMBER) {
            if (!model.get(AssistantCollectUserDataModel.REQUEST_PHONE_NUMBER_SEPARATELY)) {
                return true;
            }
            ContactModel phone_number =
                    model.get(AssistantCollectUserDataModel.SELECTED_PHONE_NUMBER);
            if (phone_number != null) {
                view.mPhoneNumberSection.addOrUpdateItem(
                        phone_number, /* select= */ true, /* notify= */ false);
            }
            // No need to reset selection if null, this will be handled by setItems().
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.TERMS_STATUS) {
            int termsStatus = model.get(AssistantCollectUserDataModel.TERMS_STATUS);
            view.mTermsSection.setTermsStatus(termsStatus);
            view.mTermsAsCheckboxSection.setTermsStatus(termsStatus);
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SELECTED_LOGIN) {
            if (!model.get(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE)) {
                return true;
            }
            LoginChoiceModel loginChoice = model.get(AssistantCollectUserDataModel.SELECTED_LOGIN);
            if (loginChoice != null) {
                view.mLoginSection.addOrUpdateItem(loginChoice,
                        /* select= */ true,
                        /* notify= */ false);
            }
            // No need to reset selection if null, this will be handled by setItems().
            return true;
        }
        return false;
    }

    private boolean updateUiState(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey == AssistantCollectUserDataModel.ENABLE_UI_INTERACTIONS) {
            boolean enabled = model.get(AssistantCollectUserDataModel.ENABLE_UI_INTERACTIONS);
            view.mContactDetailsSection.setEnabled(enabled);
            view.mPhoneNumberSection.setEnabled(enabled);
            view.mShippingAddressSection.setEnabled(enabled);
            view.mPaymentMethodSection.setEnabled(enabled);
            view.mLoginSection.setEnabled(enabled);
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.MARK_CONTACTS_LOADING) {
            view.mContactDetailsSection.setLoading(
                    model.get(AssistantCollectUserDataModel.MARK_CONTACTS_LOADING));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.MARK_PHONE_NUMBERS_LOADING) {
            view.mPhoneNumberSection.setLoading(
                    model.get(AssistantCollectUserDataModel.MARK_PHONE_NUMBERS_LOADING));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.MARK_SHIPPING_ADDRESSES_LOADING) {
            view.mShippingAddressSection.setLoading(
                    model.get(AssistantCollectUserDataModel.MARK_SHIPPING_ADDRESSES_LOADING));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.MARK_PAYMENT_METHODS_LOADING) {
            view.mPaymentMethodSection.setLoading(
                    model.get(AssistantCollectUserDataModel.MARK_PAYMENT_METHODS_LOADING));
            return true;
        }
        return false;
    }

    private boolean updateVisibilityAndPaddings(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        updateSectionVisibility(model, view);
        updateSectionPaddings(model, view);

        return propertyKey == AssistantCollectUserDataModel.REQUEST_NAME
                || propertyKey == AssistantCollectUserDataModel.REQUEST_EMAIL
                || propertyKey == AssistantCollectUserDataModel.REQUEST_PHONE
                || propertyKey == AssistantCollectUserDataModel.REQUEST_PHONE_NUMBER_SEPARATELY
                || propertyKey == AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS
                || propertyKey == AssistantCollectUserDataModel.REQUEST_PAYMENT
                || propertyKey == AssistantCollectUserDataModel.SHOW_TERMS_AS_CHECKBOX
                || propertyKey == AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE
                || propertyKey == AssistantCollectUserDataModel.EXPANDED_SECTION
                || propertyKey == AssistantCollectUserDataModel.PREPENDED_SECTIONS
                || propertyKey == AssistantCollectUserDataModel.APPENDED_SECTIONS
                || propertyKey == AssistantCollectUserDataModel.WEB_CONTENTS
                || propertyKey == AssistantCollectUserDataModel.USE_ALTERNATIVE_EDIT_DIALOGS
                || propertyKey == AssistantCollectUserDataModel.ADD_PAYMENT_INSTRUMENT_ACTION_TOKEN
                || propertyKey == AssistantCollectUserDataModel.INITIALIZE_ADDRESS_COLLECTION_PARAMS
                || propertyKey == AssistantCollectUserDataModel.AVAILABLE_CONTACTS
                || propertyKey == AssistantCollectUserDataModel.AVAILABLE_PHONE_NUMBERS
                || propertyKey == AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS
                || propertyKey == AssistantCollectUserDataModel.AVAILABLE_SHIPPING_ADDRESSES
                || propertyKey == AssistantCollectUserDataModel.AVAILABLE_LOGINS;
    }

    /**
     * Updates visibility of user data sections.
     */
    private void updateSectionVisibility(AssistantCollectUserDataModel model, ViewHolder view) {
        view.mContactDetailsSection.setVisible(shouldShowContactDetails(model));
        view.mPhoneNumberSection.setVisible(
                model.get(AssistantCollectUserDataModel.REQUEST_PHONE_NUMBER_SEPARATELY));
        view.mShippingAddressSection.setVisible(
                model.get(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS));
        view.mPaymentMethodSection.setVisible(shouldShowPaymentInstruments(model));
        if (model.get(AssistantCollectUserDataModel.SHOW_TERMS_AS_CHECKBOX)) {
            view.mTermsSection.setVisible(false);
            view.mTermsAsCheckboxSection.setVisible(true);
        } else {
            view.mTermsSection.setVisible(true);
            view.mTermsAsCheckboxSection.setVisible(false);
        }
        view.mLoginSection.setVisible(
                model.get(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE));
    }

    /**
     * Updates the paddings between sections and section dividers.
     */
    private void updateSectionPaddings(AssistantCollectUserDataModel model, ViewHolder view) {
        // Update section paddings such that the first and last section are flush to the top/bottom,
        // and all other sections have the same amount of padding in-between them.

        if (!model.get(AssistantCollectUserDataModel.PREPENDED_SECTIONS).isEmpty()) {
            view.mPrependedSections.setPaddings(
                    0, view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mLoginSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mContactDetailsSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mPhoneNumberSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mPaymentMethodSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (model.get(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE)) {
            view.mLoginSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mContactDetailsSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mPhoneNumberSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mPaymentMethodSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (shouldShowContactDetails(model)) {
            view.mContactDetailsSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mPhoneNumberSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mPaymentMethodSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (model.get(AssistantCollectUserDataModel.REQUEST_PHONE_NUMBER_SEPARATELY)) {
            view.mPhoneNumberSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mPaymentMethodSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (shouldShowPaymentInstruments(model)) {
            view.mPaymentMethodSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (model.get(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS)) {
            view.mShippingAddressSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (!model.get(AssistantCollectUserDataModel.APPENDED_SECTIONS).isEmpty()) {
            view.mAppendedSections.setPaddings(
                    0, view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        }
        view.mTermsSection.setPaddings(view.mSectionToSectionPadding, 0);
        // Do not set padding to the view.mTermsAsCheckboxSection, it already has "padding" from
        // its checkbox (that coincidentally matches the padding of mSectionToSectionPadding).
        view.mInfoSection.setPaddings(view.mSectionToSectionPadding, 0);

        // Hide dividers for currently invisible sections and after the expanded section, if any.
        boolean prevSectionIsExpandedOrInvisible = false;
        for (int i = 0; i < view.mPaymentRequestExpanderAccordion.getChildCount(); i++) {
            View child = view.mPaymentRequestExpanderAccordion.getChildAt(i);
            if (child instanceof AssistantVerticalExpander) {
                prevSectionIsExpandedOrInvisible = child.getVisibility() != View.VISIBLE
                        || (child instanceof AssistantVerticalExpander
                                && ((AssistantVerticalExpander) child).isExpanded());
            } else if (child.getTag() == view.mDividerTag) {
                child.setVisibility(prevSectionIsExpandedOrInvisible ? View.GONE : View.VISIBLE);
            } else {
                prevSectionIsExpandedOrInvisible = false;
            }
        }
    }

    /**
     * Updates/recreates section editors.
     * @return whether the property key was handled.
     */
    private boolean updateEditors(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey != AssistantCollectUserDataModel.WEB_CONTENTS
                && propertyKey != AssistantCollectUserDataModel.REQUEST_NAME
                && propertyKey != AssistantCollectUserDataModel.REQUEST_EMAIL
                && propertyKey != AssistantCollectUserDataModel.REQUEST_PHONE
                && propertyKey != AssistantCollectUserDataModel.SUPPORTED_BASIC_CARD_NETWORKS
                && propertyKey != AssistantCollectUserDataModel.SHOULD_STORE_USER_DATA_CHANGES
                && propertyKey != AssistantCollectUserDataModel.USE_ALTERNATIVE_EDIT_DIALOGS
                && propertyKey != AssistantCollectUserDataModel.ACCOUNT_EMAIL
                && propertyKey != AssistantCollectUserDataModel.ADD_PAYMENT_INSTRUMENT_ACTION_TOKEN
                && propertyKey
                        != AssistantCollectUserDataModel.INITIALIZE_ADDRESS_COLLECTION_PARAMS) {
            return false;
        }

        WebContents webContents = model.get(AssistantCollectUserDataModel.WEB_CONTENTS);
        if (webContents == null) {
            view.mContactDetailsSection.setEditor(null);
            view.mPhoneNumberSection.setEditor(null);
            view.mPaymentMethodSection.setEditor(null);
            view.mShippingAddressSection.setEditor(null);
            return true;
        }

        if (shouldShowContactDetails(model)) {
            updateContactEditor(model, view, webContents);
        }
        updatePhoneNumberEditor(model, view, webContents);
        updateAddressEditor(model, view, webContents);
        updatePaymentEditor(model, view, webContents);

        return true;
    }

    private void updateContactEditor(
            AssistantCollectUserDataModel model, ViewHolder view, WebContents webContents) {
        assert !model.get(AssistantCollectUserDataModel.USE_ALTERNATIVE_EDIT_DIALOGS)
                || !model.get(AssistantCollectUserDataModel.SHOULD_STORE_USER_DATA_CHANGES);

        // TODO(b/232484145): Create non-Autofill editors.
        // All flows reaching here must have access to Chrome dependent editors. Otherwise the
        // flow was configured wrongly.
        assert view.mEditorFactory != null;
        AssistantContactEditor editor = view.mEditorFactory.createContactEditor(webContents,
                view.mActivity, model.get(AssistantCollectUserDataModel.REQUEST_NAME),
                model.get(AssistantCollectUserDataModel.REQUEST_PHONE),
                model.get(AssistantCollectUserDataModel.REQUEST_EMAIL),
                model.get(AssistantCollectUserDataModel.SHOULD_STORE_USER_DATA_CHANGES));

        view.mContactDetailsSection.setEditor(editor);
    }

    private void updatePhoneNumberEditor(
            AssistantCollectUserDataModel model, ViewHolder view, WebContents webContents) {
        assert !model.get(AssistantCollectUserDataModel.USE_ALTERNATIVE_EDIT_DIALOGS)
                || !model.get(AssistantCollectUserDataModel.SHOULD_STORE_USER_DATA_CHANGES);

        // TODO(b/232484145): Create non-Autofill editors.
        // All flows reaching here must have access to Chrome dependent editors. Otherwise the
        // flow was configured wrongly.
        assert view.mEditorFactory != null;
        AssistantContactEditor editor =
                view.mEditorFactory.createContactEditor(webContents, view.mActivity,
                        /* requestName= */ false,
                        /* requestPhone= */ true,
                        /* requestEmail= */ false,
                        model.get(AssistantCollectUserDataModel.SHOULD_STORE_USER_DATA_CHANGES));

        view.mPhoneNumberSection.setEditor(editor);
    }

    private void updateAddressEditor(
            AssistantCollectUserDataModel model, ViewHolder view, WebContents webContents) {
        AssistantAddressEditor editor = null;
        if (model.get(AssistantCollectUserDataModel.USE_ALTERNATIVE_EDIT_DIALOGS)) {
            view.mShippingAddressSection.setRequestReloadOnChange(true);
            byte[] initializeAddressCollectionParams =
                    model.get(AssistantCollectUserDataModel.INITIALIZE_ADDRESS_COLLECTION_PARAMS);
            if (initializeAddressCollectionParams != null
                    && initializeAddressCollectionParams.length > 0) {
                editor = new AssistantAddressEditorGms(view.mActivity, view.mWindowAndroid,
                        model.get(AssistantCollectUserDataModel.ACCOUNT_EMAIL),
                        initializeAddressCollectionParams);
            }
        } else {
            view.mShippingAddressSection.setRequestReloadOnChange(false);
            // All flows reaching here must have access to Chrome dependent editors. Otherwise the
            // flow was configured wrongly.
            assert view.mEditorFactory != null;
            editor = view.mEditorFactory.createAddressEditor(webContents, view.mActivity,
                    model.get(AssistantCollectUserDataModel.SHOULD_STORE_USER_DATA_CHANGES));
        }

        view.mShippingAddressSection.setEditor(editor);
    }

    private void updatePaymentEditor(
            AssistantCollectUserDataModel model, ViewHolder view, WebContents webContents) {
        AssistantPaymentInstrumentEditor editor = null;
        if (model.get(AssistantCollectUserDataModel.USE_ALTERNATIVE_EDIT_DIALOGS)) {
            view.mPaymentMethodSection.setRequestReloadOnChange(true);
            byte[] addInstrumentActionToken =
                    model.get(AssistantCollectUserDataModel.ADD_PAYMENT_INSTRUMENT_ACTION_TOKEN);
            if (addInstrumentActionToken != null && addInstrumentActionToken.length > 0) {
                editor = new AssistantPaymentInstrumentEditorGms(view.mActivity,
                        view.mWindowAndroid, model.get(AssistantCollectUserDataModel.ACCOUNT_EMAIL),
                        addInstrumentActionToken);
            }
        } else {
            view.mPaymentMethodSection.setRequestReloadOnChange(false);
            // All flows reaching here must have access to Chrome dependent editors. Otherwise the
            // flow was configured wrongly.
            assert view.mEditorFactory != null;
            editor = view.mEditorFactory.createPaymentInstrumentEditor(webContents, view.mActivity,
                    model.get(AssistantCollectUserDataModel.SUPPORTED_BASIC_CARD_NETWORKS),
                    model.get(AssistantCollectUserDataModel.SHOULD_STORE_USER_DATA_CHANGES));
        }

        view.mPaymentMethodSection.setEditor(editor);
    }
}
