// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_proto_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace optimization_guide {

namespace {

optimization_guide::proto::AXTextAffinity TextAffinityToProto(
    ax::mojom::TextAffinity affinity) {
  switch (affinity) {
    case ax::mojom::TextAffinity::kNone:
      return optimization_guide::proto::AXTextAffinity::AX_TEXT_AFFINITY_NONE;
    case ax::mojom::TextAffinity::kDownstream:
      return optimization_guide::proto::AXTextAffinity::
          AX_TEXT_AFFINITY_DOWNSTREAM;
    case ax::mojom::TextAffinity::kUpstream:
      return optimization_guide::proto::AXTextAffinity::
          AX_TEXT_AFFINITY_UPSTREAM;
  }
}

optimization_guide::proto::AXRole RoleToProto(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kUnknown:
      return optimization_guide::proto::AXRole::AX_ROLE_UNKNOWN;
    case ax::mojom::Role::kAbbr:
      return optimization_guide::proto::AXRole::AX_ROLE_ABBR;
    case ax::mojom::Role::kAlert:
      return optimization_guide::proto::AXRole::AX_ROLE_ALERT;
    case ax::mojom::Role::kAlertDialog:
      return optimization_guide::proto::AXRole::AX_ROLE_ALERTDIALOG;
    case ax::mojom::Role::kApplication:
      return optimization_guide::proto::AXRole::AX_ROLE_APPLICATION;
    case ax::mojom::Role::kArticle:
      return optimization_guide::proto::AXRole::AX_ROLE_ARTICLE;
    case ax::mojom::Role::kAudio:
      return optimization_guide::proto::AXRole::AX_ROLE_AUDIO;
    case ax::mojom::Role::kBanner:
      return optimization_guide::proto::AXRole::AX_ROLE_BANNER;
    case ax::mojom::Role::kBlockquote:
      return optimization_guide::proto::AXRole::AX_ROLE_BLOCKQUOTE;
    case ax::mojom::Role::kButton:
      return optimization_guide::proto::AXRole::AX_ROLE_BUTTON;
    case ax::mojom::Role::kCanvas:
      return optimization_guide::proto::AXRole::AX_ROLE_CANVAS;
    case ax::mojom::Role::kCaption:
      return optimization_guide::proto::AXRole::AX_ROLE_CAPTION;
    case ax::mojom::Role::kCaret:
      return optimization_guide::proto::AXRole::AX_ROLE_CARET;
    case ax::mojom::Role::kCell:
      return optimization_guide::proto::AXRole::AX_ROLE_CELL;
    case ax::mojom::Role::kCheckBox:
      return optimization_guide::proto::AXRole::AX_ROLE_CHECKBOX;
    case ax::mojom::Role::kClient:
      return optimization_guide::proto::AXRole::AX_ROLE_CLIENT;
    case ax::mojom::Role::kCode:
      return optimization_guide::proto::AXRole::AX_ROLE_CODE;
    case ax::mojom::Role::kColorWell:
      return optimization_guide::proto::AXRole::AX_ROLE_COLORWELL;
    case ax::mojom::Role::kColumn:
      return optimization_guide::proto::AXRole::AX_ROLE_COLUMN;
    case ax::mojom::Role::kColumnHeader:
      return optimization_guide::proto::AXRole::AX_ROLE_COLUMNHEADER;
    case ax::mojom::Role::kComboBoxGrouping:
      return optimization_guide::proto::AXRole::AX_ROLE_COMBOBOXGROUPING;
    case ax::mojom::Role::kComboBoxMenuButton:
      return optimization_guide::proto::AXRole::AX_ROLE_COMBOBOXMENUBUTTON;
    case ax::mojom::Role::kComboBoxSelect:
      return optimization_guide::proto::AXRole::AX_ROLE_COMBOBOXSELECT;
    case ax::mojom::Role::kComplementary:
      return optimization_guide::proto::AXRole::AX_ROLE_COMPLEMENTARY;
    case ax::mojom::Role::kComment:
      return optimization_guide::proto::AXRole::AX_ROLE_COMMENT;
    case ax::mojom::Role::kContentDeletion:
      return optimization_guide::proto::AXRole::AX_ROLE_CONTENTDELETION;
    case ax::mojom::Role::kContentInsertion:
      return optimization_guide::proto::AXRole::AX_ROLE_CONTENTINSERTION;
    case ax::mojom::Role::kContentInfo:
      return optimization_guide::proto::AXRole::AX_ROLE_CONTENTINFO;
    case ax::mojom::Role::kDate:
      return optimization_guide::proto::AXRole::AX_ROLE_DATE;
    case ax::mojom::Role::kDateTime:
      return optimization_guide::proto::AXRole::AX_ROLE_DATETIME;
    case ax::mojom::Role::kDefinition:
      return optimization_guide::proto::AXRole::AX_ROLE_DEFINITION;
    case ax::mojom::Role::kDescriptionList:
      return optimization_guide::proto::AXRole::AX_ROLE_DESCRIPTIONLIST;
    case ax::mojom::Role::kDescriptionListDetailDeprecated:
      return optimization_guide::proto::AXRole::
          AX_ROLE_DESCRIPTIONLISTDETAILDEPRECATED;
    case ax::mojom::Role::kDescriptionListTermDeprecated:
      return optimization_guide::proto::AXRole::
          AX_ROLE_DESCRIPTIONLISTTERMDEPRECATED;
    case ax::mojom::Role::kDesktop:
      return optimization_guide::proto::AXRole::AX_ROLE_DESKTOP;
    case ax::mojom::Role::kDetails:
      return optimization_guide::proto::AXRole::AX_ROLE_DETAILS;
    case ax::mojom::Role::kDialog:
      return optimization_guide::proto::AXRole::AX_ROLE_DIALOG;
    case ax::mojom::Role::kDirectoryDeprecated:
      return optimization_guide::proto::AXRole::AX_ROLE_DIRECTORYDEPRECATED;
    case ax::mojom::Role::kDisclosureTriangle:
      return optimization_guide::proto::AXRole::AX_ROLE_DISCLOSURETRIANGLE;
    case ax::mojom::Role::kDisclosureTriangleGrouped:
      return optimization_guide::proto::AXRole::
          AX_ROLE_DISCLOSURETRIANGLEGROUPED;
    case ax::mojom::Role::kDocAbstract:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCABSTRACT;
    case ax::mojom::Role::kDocAcknowledgments:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCACKNOWLEDGMENTS;
    case ax::mojom::Role::kDocAfterword:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCAFTERWORD;
    case ax::mojom::Role::kDocAppendix:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCAPPENDIX;
    case ax::mojom::Role::kDocBackLink:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCBACKLINK;
    case ax::mojom::Role::kDocBiblioEntry:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCBIBLIOENTRY;
    case ax::mojom::Role::kDocBibliography:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCBIBLIOGRAPHY;
    case ax::mojom::Role::kDocBiblioRef:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCBIBLIOREF;
    case ax::mojom::Role::kDocChapter:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCCHAPTER;
    case ax::mojom::Role::kDocColophon:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCCOLOPHON;
    case ax::mojom::Role::kDocConclusion:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCCONCLUSION;
    case ax::mojom::Role::kDocCover:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCCOVER;
    case ax::mojom::Role::kDocCredit:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCCREDIT;
    case ax::mojom::Role::kDocCredits:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCCREDITS;
    case ax::mojom::Role::kDocDedication:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCDEDICATION;
    case ax::mojom::Role::kDocEndnote:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCENDNOTE;
    case ax::mojom::Role::kDocEndnotes:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCENDNOTES;
    case ax::mojom::Role::kDocEpigraph:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCEPIGRAPH;
    case ax::mojom::Role::kDocEpilogue:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCEPILOGUE;
    case ax::mojom::Role::kDocErrata:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCERRATA;
    case ax::mojom::Role::kDocExample:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCEXAMPLE;
    case ax::mojom::Role::kDocFootnote:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCFOOTNOTE;
    case ax::mojom::Role::kDocForeword:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCFOREWORD;
    case ax::mojom::Role::kDocGlossary:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCGLOSSARY;
    case ax::mojom::Role::kDocGlossRef:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCGLOSSREF;
    case ax::mojom::Role::kDocIndex:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCINDEX;
    case ax::mojom::Role::kDocIntroduction:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCINTRODUCTION;
    case ax::mojom::Role::kDocNoteRef:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCNOTEREF;
    case ax::mojom::Role::kDocNotice:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCNOTICE;
    case ax::mojom::Role::kDocPageBreak:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCPAGEBREAK;
    case ax::mojom::Role::kDocPageFooter:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCPAGEFOOTER;
    case ax::mojom::Role::kDocPageHeader:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCPAGEHEADER;
    case ax::mojom::Role::kDocPageList:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCPAGELIST;
    case ax::mojom::Role::kDocPart:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCPART;
    case ax::mojom::Role::kDocPreface:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCPREFACE;
    case ax::mojom::Role::kDocPrologue:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCPROLOGUE;
    case ax::mojom::Role::kDocPullquote:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCPULLQUOTE;
    case ax::mojom::Role::kDocQna:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCQNA;
    case ax::mojom::Role::kDocSubtitle:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCSUBTITLE;
    case ax::mojom::Role::kDocTip:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCTIP;
    case ax::mojom::Role::kDocToc:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCTOC;
    case ax::mojom::Role::kDocument:
      return optimization_guide::proto::AXRole::AX_ROLE_DOCUMENT;
    case ax::mojom::Role::kEmbeddedObject:
      return optimization_guide::proto::AXRole::AX_ROLE_EMBEDDEDOBJECT;
    case ax::mojom::Role::kEmphasis:
      return optimization_guide::proto::AXRole::AX_ROLE_EMPHASIS;
    case ax::mojom::Role::kFeed:
      return optimization_guide::proto::AXRole::AX_ROLE_FEED;
    case ax::mojom::Role::kFigcaption:
      return optimization_guide::proto::AXRole::AX_ROLE_FIGCAPTION;
    case ax::mojom::Role::kFigure:
      return optimization_guide::proto::AXRole::AX_ROLE_FIGURE;
    case ax::mojom::Role::kFooter:
      return optimization_guide::proto::AXRole::AX_ROLE_FOOTER;
    case ax::mojom::Role::kForm:
      return optimization_guide::proto::AXRole::AX_ROLE_FORM;
    case ax::mojom::Role::kGenericContainer:
      return optimization_guide::proto::AXRole::AX_ROLE_GENERICCONTAINER;
    case ax::mojom::Role::kGraphicsDocument:
      return optimization_guide::proto::AXRole::AX_ROLE_GRAPHICSDOCUMENT;
    case ax::mojom::Role::kGraphicsObject:
      return optimization_guide::proto::AXRole::AX_ROLE_GRAPHICSOBJECT;
    case ax::mojom::Role::kGraphicsSymbol:
      return optimization_guide::proto::AXRole::AX_ROLE_GRAPHICSSYMBOL;
    case ax::mojom::Role::kGrid:
      return optimization_guide::proto::AXRole::AX_ROLE_GRID;
    case ax::mojom::Role::kGridCell:
      return optimization_guide::proto::AXRole::AX_ROLE_GRIDCELL;
    case ax::mojom::Role::kGroup:
      return optimization_guide::proto::AXRole::AX_ROLE_GROUP;
    case ax::mojom::Role::kHeader:
      return optimization_guide::proto::AXRole::AX_ROLE_HEADER;
    case ax::mojom::Role::kHeading:
      return optimization_guide::proto::AXRole::AX_ROLE_HEADING;
    case ax::mojom::Role::kIframe:
      return optimization_guide::proto::AXRole::AX_ROLE_IFRAME;
    case ax::mojom::Role::kIframePresentational:
      return optimization_guide::proto::AXRole::AX_ROLE_IFRAMEPRESENTATIONAL;
    case ax::mojom::Role::kImage:
      return optimization_guide::proto::AXRole::AX_ROLE_IMAGE;
    case ax::mojom::Role::kImeCandidate:
      return optimization_guide::proto::AXRole::AX_ROLE_IMECANDIDATE;
    case ax::mojom::Role::kInlineTextBox:
      return optimization_guide::proto::AXRole::AX_ROLE_INLINETEXTBOX;
    case ax::mojom::Role::kInputTime:
      return optimization_guide::proto::AXRole::AX_ROLE_INPUTTIME;
    case ax::mojom::Role::kKeyboard:
      return optimization_guide::proto::AXRole::AX_ROLE_KEYBOARD;
    case ax::mojom::Role::kLabelText:
      return optimization_guide::proto::AXRole::AX_ROLE_LABELTEXT;
    case ax::mojom::Role::kLayoutTable:
      return optimization_guide::proto::AXRole::AX_ROLE_LAYOUTTABLE;
    case ax::mojom::Role::kLayoutTableCell:
      return optimization_guide::proto::AXRole::AX_ROLE_LAYOUTTABLECELL;
    case ax::mojom::Role::kLayoutTableRow:
      return optimization_guide::proto::AXRole::AX_ROLE_LAYOUTTABLEROW;
    case ax::mojom::Role::kLegend:
      return optimization_guide::proto::AXRole::AX_ROLE_LEGEND;
    case ax::mojom::Role::kLineBreak:
      return optimization_guide::proto::AXRole::AX_ROLE_LINEBREAK;
    case ax::mojom::Role::kLink:
      return optimization_guide::proto::AXRole::AX_ROLE_LINK;
    case ax::mojom::Role::kList:
      return optimization_guide::proto::AXRole::AX_ROLE_LIST;
    case ax::mojom::Role::kListBox:
      return optimization_guide::proto::AXRole::AX_ROLE_LISTBOX;
    case ax::mojom::Role::kListBoxOption:
      return optimization_guide::proto::AXRole::AX_ROLE_LISTBOXOPTION;
    case ax::mojom::Role::kListGrid:
      return optimization_guide::proto::AXRole::AX_ROLE_LISTGRID;
    case ax::mojom::Role::kListItem:
      return optimization_guide::proto::AXRole::AX_ROLE_LISTITEM;
    case ax::mojom::Role::kListMarker:
      return optimization_guide::proto::AXRole::AX_ROLE_LISTMARKER;
    case ax::mojom::Role::kLog:
      return optimization_guide::proto::AXRole::AX_ROLE_LOG;
    case ax::mojom::Role::kMain:
      return optimization_guide::proto::AXRole::AX_ROLE_MAIN;
    case ax::mojom::Role::kMark:
      return optimization_guide::proto::AXRole::AX_ROLE_MARK;
    case ax::mojom::Role::kMarquee:
      return optimization_guide::proto::AXRole::AX_ROLE_MARQUEE;
    case ax::mojom::Role::kMath:
      return optimization_guide::proto::AXRole::AX_ROLE_MATH;
    case ax::mojom::Role::kMathMLFraction:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLFRACTION;
    case ax::mojom::Role::kMathMLIdentifier:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLIDENTIFIER;
    case ax::mojom::Role::kMathMLMath:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLMATH;
    case ax::mojom::Role::kMathMLMultiscripts:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLMULTISCRIPTS;
    case ax::mojom::Role::kMathMLNoneScript:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLNONESCRIPT;
    case ax::mojom::Role::kMathMLNumber:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLNUMBER;
    case ax::mojom::Role::kMathMLOperator:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLOPERATOR;
    case ax::mojom::Role::kMathMLOver:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLOVER;
    case ax::mojom::Role::kMathMLPrescriptDelimiter:
      return optimization_guide::proto::AXRole::
          AX_ROLE_MATHMLPRESCRIPTDELIMITER;
    case ax::mojom::Role::kMathMLRoot:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLROOT;
    case ax::mojom::Role::kMathMLRow:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLROW;
    case ax::mojom::Role::kMathMLSquareRoot:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLSQUAREROOT;
    case ax::mojom::Role::kMathMLStringLiteral:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLSTRINGLITERAL;
    case ax::mojom::Role::kMathMLSub:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLSUB;
    case ax::mojom::Role::kMathMLSubSup:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLSUBSUP;
    case ax::mojom::Role::kMathMLSup:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLSUP;
    case ax::mojom::Role::kMathMLTable:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLTABLE;
    case ax::mojom::Role::kMathMLTableCell:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLTABLECELL;
    case ax::mojom::Role::kMathMLTableRow:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLTABLEROW;
    case ax::mojom::Role::kMathMLText:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLTEXT;
    case ax::mojom::Role::kMathMLUnder:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLUNDER;
    case ax::mojom::Role::kMathMLUnderOver:
      return optimization_guide::proto::AXRole::AX_ROLE_MATHMLUNDEROVER;
    case ax::mojom::Role::kMenu:
      return optimization_guide::proto::AXRole::AX_ROLE_MENU;
    case ax::mojom::Role::kMenuBar:
      return optimization_guide::proto::AXRole::AX_ROLE_MENUBAR;
    case ax::mojom::Role::kMenuItem:
      return optimization_guide::proto::AXRole::AX_ROLE_MENUITEM;
    case ax::mojom::Role::kMenuItemCheckBox:
      return optimization_guide::proto::AXRole::AX_ROLE_MENUITEMCHECKBOX;
    case ax::mojom::Role::kMenuItemRadio:
      return optimization_guide::proto::AXRole::AX_ROLE_MENUITEMRADIO;
    case ax::mojom::Role::kMenuListOption:
      return optimization_guide::proto::AXRole::AX_ROLE_MENULISTOPTION;
    case ax::mojom::Role::kMenuListPopup:
      return optimization_guide::proto::AXRole::AX_ROLE_MENULISTPOPUP;
    case ax::mojom::Role::kMeter:
      return optimization_guide::proto::AXRole::AX_ROLE_METER;
    case ax::mojom::Role::kNavigation:
      return optimization_guide::proto::AXRole::AX_ROLE_NAVIGATION;
    case ax::mojom::Role::kNone:
      return optimization_guide::proto::AXRole::AX_ROLE_NONE;
    case ax::mojom::Role::kNote:
      return optimization_guide::proto::AXRole::AX_ROLE_NOTE;
    case ax::mojom::Role::kPane:
      return optimization_guide::proto::AXRole::AX_ROLE_PANE;
    case ax::mojom::Role::kParagraph:
      return optimization_guide::proto::AXRole::AX_ROLE_PARAGRAPH;
    case ax::mojom::Role::kPdfActionableHighlight:
      return optimization_guide::proto::AXRole::AX_ROLE_PDFACTIONABLEHIGHLIGHT;
    case ax::mojom::Role::kPdfRoot:
      return optimization_guide::proto::AXRole::AX_ROLE_PDFROOT;
    case ax::mojom::Role::kPluginObject:
      return optimization_guide::proto::AXRole::AX_ROLE_PLUGINOBJECT;
    case ax::mojom::Role::kPopUpButton:
      return optimization_guide::proto::AXRole::AX_ROLE_POPUPBUTTON;
    case ax::mojom::Role::kPortalDeprecated:
      return optimization_guide::proto::AXRole::AX_ROLE_PORTALDEPRECATED;
    case ax::mojom::Role::kPreDeprecated:
      return optimization_guide::proto::AXRole::AX_ROLE_PREDEPRECATED;
    case ax::mojom::Role::kProgressIndicator:
      return optimization_guide::proto::AXRole::AX_ROLE_PROGRESSINDICATOR;
    case ax::mojom::Role::kRadioButton:
      return optimization_guide::proto::AXRole::AX_ROLE_RADIOBUTTON;
    case ax::mojom::Role::kRadioGroup:
      return optimization_guide::proto::AXRole::AX_ROLE_RADIOGROUP;
    case ax::mojom::Role::kRegion:
      return optimization_guide::proto::AXRole::AX_ROLE_REGION;
    case ax::mojom::Role::kRootWebArea:
      return optimization_guide::proto::AXRole::AX_ROLE_ROOTWEBAREA;
    case ax::mojom::Role::kRow:
      return optimization_guide::proto::AXRole::AX_ROLE_ROW;
    case ax::mojom::Role::kRowGroup:
      return optimization_guide::proto::AXRole::AX_ROLE_ROWGROUP;
    case ax::mojom::Role::kRowHeader:
      return optimization_guide::proto::AXRole::AX_ROLE_ROWHEADER;
    case ax::mojom::Role::kRuby:
      return optimization_guide::proto::AXRole::AX_ROLE_RUBY;
    case ax::mojom::Role::kRubyAnnotation:
      return optimization_guide::proto::AXRole::AX_ROLE_RUBYANNOTATION;
    case ax::mojom::Role::kScrollBar:
      return optimization_guide::proto::AXRole::AX_ROLE_SCROLLBAR;
    case ax::mojom::Role::kScrollView:
      return optimization_guide::proto::AXRole::AX_ROLE_SCROLLVIEW;
    case ax::mojom::Role::kSearch:
      return optimization_guide::proto::AXRole::AX_ROLE_SEARCH;
    case ax::mojom::Role::kSearchBox:
      return optimization_guide::proto::AXRole::AX_ROLE_SEARCHBOX;
    case ax::mojom::Role::kSection:
      return optimization_guide::proto::AXRole::AX_ROLE_SECTION;
    case ax::mojom::Role::kSectionFooter:
      return optimization_guide::proto::AXRole::AX_ROLE_SECTIONFOOTER;
    case ax::mojom::Role::kSectionHeader:
      return optimization_guide::proto::AXRole::AX_ROLE_SECTIONHEADER;
    case ax::mojom::Role::kSectionWithoutName:
      return optimization_guide::proto::AXRole::AX_ROLE_SECTIONWITHOUTNAME;
    case ax::mojom::Role::kSlider:
      return optimization_guide::proto::AXRole::AX_ROLE_SLIDER;
    case ax::mojom::Role::kSpinButton:
      return optimization_guide::proto::AXRole::AX_ROLE_SPINBUTTON;
    case ax::mojom::Role::kSplitter:
      return optimization_guide::proto::AXRole::AX_ROLE_SPLITTER;
    case ax::mojom::Role::kStaticText:
      return optimization_guide::proto::AXRole::AX_ROLE_STATICTEXT;
    case ax::mojom::Role::kStatus:
      return optimization_guide::proto::AXRole::AX_ROLE_STATUS;
    case ax::mojom::Role::kStrong:
      return optimization_guide::proto::AXRole::AX_ROLE_STRONG;
    case ax::mojom::Role::kSubscript:
      return optimization_guide::proto::AXRole::AX_ROLE_SUBSCRIPT;
    case ax::mojom::Role::kSuggestion:
      return optimization_guide::proto::AXRole::AX_ROLE_SUGGESTION;
    case ax::mojom::Role::kSuperscript:
      return optimization_guide::proto::AXRole::AX_ROLE_SUPERSCRIPT;
    case ax::mojom::Role::kSvgRoot:
      return optimization_guide::proto::AXRole::AX_ROLE_SVGROOT;
    case ax::mojom::Role::kSwitch:
      return optimization_guide::proto::AXRole::AX_ROLE_SWITCH;
    case ax::mojom::Role::kTab:
      return optimization_guide::proto::AXRole::AX_ROLE_TAB;
    case ax::mojom::Role::kTabList:
      return optimization_guide::proto::AXRole::AX_ROLE_TABLIST;
    case ax::mojom::Role::kTabPanel:
      return optimization_guide::proto::AXRole::AX_ROLE_TABPANEL;
    case ax::mojom::Role::kTable:
      return optimization_guide::proto::AXRole::AX_ROLE_TABLE;
    case ax::mojom::Role::kTableHeaderContainer:
      return optimization_guide::proto::AXRole::AX_ROLE_TABLEHEADERCONTAINER;
    case ax::mojom::Role::kTerm:
      return optimization_guide::proto::AXRole::AX_ROLE_TERM;
    case ax::mojom::Role::kTextField:
      return optimization_guide::proto::AXRole::AX_ROLE_TEXTFIELD;
    case ax::mojom::Role::kTextFieldWithComboBox:
      return optimization_guide::proto::AXRole::AX_ROLE_TEXTFIELDWITHCOMBOBOX;
    case ax::mojom::Role::kTime:
      return optimization_guide::proto::AXRole::AX_ROLE_TIME;
    case ax::mojom::Role::kTimer:
      return optimization_guide::proto::AXRole::AX_ROLE_TIMER;
    case ax::mojom::Role::kTitleBar:
      return optimization_guide::proto::AXRole::AX_ROLE_TITLEBAR;
    case ax::mojom::Role::kToggleButton:
      return optimization_guide::proto::AXRole::AX_ROLE_TOGGLEBUTTON;
    case ax::mojom::Role::kToolbar:
      return optimization_guide::proto::AXRole::AX_ROLE_TOOLBAR;
    case ax::mojom::Role::kTooltip:
      return optimization_guide::proto::AXRole::AX_ROLE_TOOLTIP;
    case ax::mojom::Role::kTree:
      return optimization_guide::proto::AXRole::AX_ROLE_TREE;
    case ax::mojom::Role::kTreeGrid:
      return optimization_guide::proto::AXRole::AX_ROLE_TREEGRID;
    case ax::mojom::Role::kTreeItem:
      return optimization_guide::proto::AXRole::AX_ROLE_TREEITEM;
    case ax::mojom::Role::kVideo:
      return optimization_guide::proto::AXRole::AX_ROLE_VIDEO;
    case ax::mojom::Role::kWebView:
      return optimization_guide::proto::AXRole::AX_ROLE_WEBVIEW;
    case ax::mojom::Role::kWindow:
      return optimization_guide::proto::AXRole::AX_ROLE_WINDOW;
  }
}

optimization_guide::proto::AXStringAttribute StringAttributeToProto(
    ax::mojom::StringAttribute attribute) {
  switch (attribute) {
    case ax::mojom::StringAttribute::kNone:
      return optimization_guide::proto::AXStringAttribute::AX_SA_NONE;
    case ax::mojom::StringAttribute::kAccessKey:
      return optimization_guide::proto::AXStringAttribute::AX_SA_ACCESSKEY;
    case ax::mojom::StringAttribute::kAppId:
      return optimization_guide::proto::AXStringAttribute::AX_SA_APPID;
    case ax::mojom::StringAttribute::kAriaInvalidValueDeprecated:
      return optimization_guide::proto::AXStringAttribute::
          AX_SA_ARIAINVALIDVALUEDEPRECATED;
    case ax::mojom::StringAttribute::kAutoComplete:
      return optimization_guide::proto::AXStringAttribute::AX_SA_AUTOCOMPLETE;
    case ax::mojom::StringAttribute::kAriaBrailleLabel:
      return optimization_guide::proto::AXStringAttribute::
          AX_SA_ARIABRAILLELABEL;
    case ax::mojom::StringAttribute::kAriaBrailleRoleDescription:
      return optimization_guide::proto::AXStringAttribute::
          AX_SA_ARIABRAILLEROLEDESCRIPTION;
    case ax::mojom::StringAttribute::kAriaCellColumnIndexText:
      return optimization_guide::proto::AXStringAttribute::
          AX_SA_ARIACELLCOLUMNINDEXTEXT;
    case ax::mojom::StringAttribute::kAriaCellRowIndexText:
      return optimization_guide::proto::AXStringAttribute::
          AX_SA_ARIACELLROWINDEXTEXT;
    case ax::mojom::StringAttribute::kAriaNotificationAnnouncementDeprecated:
      return optimization_guide::proto::AXStringAttribute::
          AX_SA_ARIANOTIFICATIONANNOUNCEMENTDEPRECATED;
    case ax::mojom::StringAttribute::kAriaNotificationIdDeprecated:
      return optimization_guide::proto::AXStringAttribute::
          AX_SA_ARIANOTIFICATIONIDDEPRECATED;
    case ax::mojom::StringAttribute::kCheckedStateDescription:
      return optimization_guide::proto::AXStringAttribute::
          AX_SA_CHECKEDSTATEDESCRIPTION;
    case ax::mojom::StringAttribute::kChildTreeId:
      return optimization_guide::proto::AXStringAttribute::AX_SA_CHILDTREEID;
    case ax::mojom::StringAttribute::kChildTreeNodeAppId:
      return optimization_guide::proto::AXStringAttribute::
          AX_SA_CHILDTREENODEAPPID;
    case ax::mojom::StringAttribute::kClassName:
      return optimization_guide::proto::AXStringAttribute::AX_SA_CLASSNAME;
    case ax::mojom::StringAttribute::kContainerLiveRelevant:
      return optimization_guide::proto::AXStringAttribute::
          AX_SA_CONTAINERLIVERELEVANT;
    case ax::mojom::StringAttribute::kContainerLiveStatus:
      return optimization_guide::proto::AXStringAttribute::
          AX_SA_CONTAINERLIVESTATUS;
    case ax::mojom::StringAttribute::kDescription:
      return optimization_guide::proto::AXStringAttribute::AX_SA_DESCRIPTION;
    case ax::mojom::StringAttribute::kDisplay:
      return optimization_guide::proto::AXStringAttribute::AX_SA_DISPLAY;
    case ax::mojom::StringAttribute::kDoDefaultLabel:
      return optimization_guide::proto::AXStringAttribute::AX_SA_DODEFAULTLABEL;
    case ax::mojom::StringAttribute::kFontFamily:
      return optimization_guide::proto::AXStringAttribute::AX_SA_FONTFAMILY;
    case ax::mojom::StringAttribute::kHtmlId:
      return optimization_guide::proto::AXStringAttribute::AX_SA_HTMLID;
    case ax::mojom::StringAttribute::kHtmlTag:
      return optimization_guide::proto::AXStringAttribute::AX_SA_HTMLTAG;
    case ax::mojom::StringAttribute::kImageAnnotation:
      return optimization_guide::proto::AXStringAttribute::
          AX_SA_IMAGEANNOTATION;
    case ax::mojom::StringAttribute::kImageDataUrl:
      return optimization_guide::proto::AXStringAttribute::AX_SA_IMAGEDATAURL;
    case ax::mojom::StringAttribute::kMathContent:
      return optimization_guide::proto::AXStringAttribute::AX_SA_MATHCONTENT;
    case ax::mojom::StringAttribute::kInputType:
      return optimization_guide::proto::AXStringAttribute::AX_SA_INPUTTYPE;
    case ax::mojom::StringAttribute::kKeyShortcuts:
      return optimization_guide::proto::AXStringAttribute::AX_SA_KEYSHORTCUTS;
    case ax::mojom::StringAttribute::kLanguage:
      return optimization_guide::proto::AXStringAttribute::AX_SA_LANGUAGE;
    case ax::mojom::StringAttribute::kLinkTarget:
      return optimization_guide::proto::AXStringAttribute::AX_SA_LINKTARGET;
    case ax::mojom::StringAttribute::kLongClickLabel:
      return optimization_guide::proto::AXStringAttribute::AX_SA_LONGCLICKLABEL;
    case ax::mojom::StringAttribute::kName:
      return optimization_guide::proto::AXStringAttribute::AX_SA_NAME;
    case ax::mojom::StringAttribute::kLiveRelevant:
      return optimization_guide::proto::AXStringAttribute::AX_SA_LIVERELEVANT;
    case ax::mojom::StringAttribute::kLiveStatus:
      return optimization_guide::proto::AXStringAttribute::AX_SA_LIVESTATUS;
    case ax::mojom::StringAttribute::kPlaceholder:
      return optimization_guide::proto::AXStringAttribute::AX_SA_PLACEHOLDER;
    case ax::mojom::StringAttribute::kRole:
      return optimization_guide::proto::AXStringAttribute::AX_SA_ROLE;
    case ax::mojom::StringAttribute::kRoleDescription:
      return optimization_guide::proto::AXStringAttribute::
          AX_SA_ROLEDESCRIPTION;
    case ax::mojom::StringAttribute::kTooltip:
      return optimization_guide::proto::AXStringAttribute::AX_SA_TOOLTIP;
    case ax::mojom::StringAttribute::kUrl:
      return optimization_guide::proto::AXStringAttribute::AX_SA_URL;
    case ax::mojom::StringAttribute::kValue:
      return optimization_guide::proto::AXStringAttribute::AX_SA_VALUE;
    case ax::mojom::StringAttribute::kVirtualContent:
      return optimization_guide::proto::AXStringAttribute::AX_SA_VIRTUALCONTENT;
  }
}

optimization_guide::proto::AXIntAttribute IntAttributeToProto(
    ax::mojom::IntAttribute attribute) {
  switch (attribute) {
    case ax::mojom::IntAttribute::kNone:
      return optimization_guide::proto::AXIntAttribute::AX_IA_NONE;
    case ax::mojom::IntAttribute::kDefaultActionVerb:
      return optimization_guide::proto::AXIntAttribute::AX_IA_DEFAULTACTIONVERB;
    case ax::mojom::IntAttribute::kScrollX:
      return optimization_guide::proto::AXIntAttribute::AX_IA_SCROLLX;
    case ax::mojom::IntAttribute::kScrollXMin:
      return optimization_guide::proto::AXIntAttribute::AX_IA_SCROLLXMIN;
    case ax::mojom::IntAttribute::kScrollXMax:
      return optimization_guide::proto::AXIntAttribute::AX_IA_SCROLLXMAX;
    case ax::mojom::IntAttribute::kScrollY:
      return optimization_guide::proto::AXIntAttribute::AX_IA_SCROLLY;
    case ax::mojom::IntAttribute::kScrollYMin:
      return optimization_guide::proto::AXIntAttribute::AX_IA_SCROLLYMIN;
    case ax::mojom::IntAttribute::kScrollYMax:
      return optimization_guide::proto::AXIntAttribute::AX_IA_SCROLLYMAX;
    case ax::mojom::IntAttribute::kTextSelStart:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TEXTSELSTART;
    case ax::mojom::IntAttribute::kTextSelEnd:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TEXTSELEND;
    case ax::mojom::IntAttribute::kAriaColumnCount:
      return optimization_guide::proto::AXIntAttribute::AX_IA_ARIACOLUMNCOUNT;
    case ax::mojom::IntAttribute::kAriaCellColumnIndex:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_ARIACELLCOLUMNINDEX;
    case ax::mojom::IntAttribute::kAriaCellColumnSpan:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_ARIACELLCOLUMNSPAN;
    case ax::mojom::IntAttribute::kAriaRowCount:
      return optimization_guide::proto::AXIntAttribute::AX_IA_ARIAROWCOUNT;
    case ax::mojom::IntAttribute::kAriaCellRowIndex:
      return optimization_guide::proto::AXIntAttribute::AX_IA_ARIACELLROWINDEX;
    case ax::mojom::IntAttribute::kAriaCellRowSpan:
      return optimization_guide::proto::AXIntAttribute::AX_IA_ARIACELLROWSPAN;
    case ax::mojom::IntAttribute::kTableRowCount:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TABLEROWCOUNT;
    case ax::mojom::IntAttribute::kTableColumnCount:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TABLECOLUMNCOUNT;
    case ax::mojom::IntAttribute::kTableHeaderId:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TABLEHEADERID;
    case ax::mojom::IntAttribute::kTableRowIndex:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TABLEROWINDEX;
    case ax::mojom::IntAttribute::kTableRowHeaderId:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TABLEROWHEADERID;
    case ax::mojom::IntAttribute::kTableColumnIndex:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TABLECOLUMNINDEX;
    case ax::mojom::IntAttribute::kTableColumnHeaderId:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_TABLECOLUMNHEADERID;
    case ax::mojom::IntAttribute::kTableCellColumnIndex:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_TABLECELLCOLUMNINDEX;
    case ax::mojom::IntAttribute::kTableCellColumnSpan:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_TABLECELLCOLUMNSPAN;
    case ax::mojom::IntAttribute::kTableCellRowIndex:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TABLECELLROWINDEX;
    case ax::mojom::IntAttribute::kTableCellRowSpan:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TABLECELLROWSPAN;
    case ax::mojom::IntAttribute::kSortDirection:
      return optimization_guide::proto::AXIntAttribute::AX_IA_SORTDIRECTION;
    case ax::mojom::IntAttribute::kHierarchicalLevel:
      return optimization_guide::proto::AXIntAttribute::AX_IA_HIERARCHICALLEVEL;
    case ax::mojom::IntAttribute::kNameFrom:
      return optimization_guide::proto::AXIntAttribute::AX_IA_NAMEFROM;
    case ax::mojom::IntAttribute::kDescriptionFrom:
      return optimization_guide::proto::AXIntAttribute::AX_IA_DESCRIPTIONFROM;
    case ax::mojom::IntAttribute::kDetailsFrom:
      return optimization_guide::proto::AXIntAttribute::AX_IA_DETAILSFROM;
    case ax::mojom::IntAttribute::kActivedescendantId:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_ACTIVEDESCENDANTID;
    case ax::mojom::IntAttribute::kErrormessageIdDeprecated:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_ERRORMESSAGEIDDEPRECATED;
    case ax::mojom::IntAttribute::kInPageLinkTargetId:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_INPAGELINKTARGETID;
    case ax::mojom::IntAttribute::kMemberOfId:
      return optimization_guide::proto::AXIntAttribute::AX_IA_MEMBEROFID;
    case ax::mojom::IntAttribute::kNextOnLineId:
      return optimization_guide::proto::AXIntAttribute::AX_IA_NEXTONLINEID;
    case ax::mojom::IntAttribute::kPopupForId:
      return optimization_guide::proto::AXIntAttribute::AX_IA_POPUPFORID;
    case ax::mojom::IntAttribute::kPreviousOnLineId:
      return optimization_guide::proto::AXIntAttribute::AX_IA_PREVIOUSONLINEID;
    case ax::mojom::IntAttribute::kRestriction:
      return optimization_guide::proto::AXIntAttribute::AX_IA_RESTRICTION;
    case ax::mojom::IntAttribute::kSetSize:
      return optimization_guide::proto::AXIntAttribute::AX_IA_SETSIZE;
    case ax::mojom::IntAttribute::kPosInSet:
      return optimization_guide::proto::AXIntAttribute::AX_IA_POSINSET;
    case ax::mojom::IntAttribute::kColorValue:
      return optimization_guide::proto::AXIntAttribute::AX_IA_COLORVALUE;
    case ax::mojom::IntAttribute::kAriaCurrentState:
      return optimization_guide::proto::AXIntAttribute::AX_IA_ARIACURRENTSTATE;
    case ax::mojom::IntAttribute::kBackgroundColor:
      return optimization_guide::proto::AXIntAttribute::AX_IA_BACKGROUNDCOLOR;
    case ax::mojom::IntAttribute::kColor:
      return optimization_guide::proto::AXIntAttribute::AX_IA_COLOR;
    case ax::mojom::IntAttribute::kHasPopup:
      return optimization_guide::proto::AXIntAttribute::AX_IA_HASPOPUP;
    case ax::mojom::IntAttribute::kImageAnnotationStatus:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_IMAGEANNOTATIONSTATUS;
    case ax::mojom::IntAttribute::kInvalidState:
      return optimization_guide::proto::AXIntAttribute::AX_IA_INVALIDSTATE;
    case ax::mojom::IntAttribute::kCheckedState:
      return optimization_guide::proto::AXIntAttribute::AX_IA_CHECKEDSTATE;
    case ax::mojom::IntAttribute::kListStyle:
      return optimization_guide::proto::AXIntAttribute::AX_IA_LISTSTYLE;
    case ax::mojom::IntAttribute::kTextAlign:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TEXTALIGN;
    case ax::mojom::IntAttribute::kTextDirection:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TEXTDIRECTION;
    case ax::mojom::IntAttribute::kTextPosition:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TEXTPOSITION;
    case ax::mojom::IntAttribute::kTextStyle:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TEXTSTYLE;
    case ax::mojom::IntAttribute::kTextOverlineStyle:
      return optimization_guide::proto::AXIntAttribute::AX_IA_TEXTOVERLINESTYLE;
    case ax::mojom::IntAttribute::kTextStrikethroughStyle:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_TEXTSTRIKETHROUGHSTYLE;
    case ax::mojom::IntAttribute::kTextUnderlineStyle:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_TEXTUNDERLINESTYLE;
    case ax::mojom::IntAttribute::kPreviousFocusId:
      return optimization_guide::proto::AXIntAttribute::AX_IA_PREVIOUSFOCUSID;
    case ax::mojom::IntAttribute::kNextFocusId:
      return optimization_guide::proto::AXIntAttribute::AX_IA_NEXTFOCUSID;
    case ax::mojom::IntAttribute::kDropeffectDeprecated:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_DROPEFFECTDEPRECATED;
    case ax::mojom::IntAttribute::kDOMNodeIdDeprecated:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_DOMNODEIDDEPRECATED;
    case ax::mojom::IntAttribute::kIsPopup:
      return optimization_guide::proto::AXIntAttribute::AX_IA_ISPOPUP;
    case ax::mojom::IntAttribute::kNextWindowFocusId:
      return optimization_guide::proto::AXIntAttribute::AX_IA_NEXTWINDOWFOCUSID;
    case ax::mojom::IntAttribute::kPreviousWindowFocusId:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_PREVIOUSWINDOWFOCUSID;
    case ax::mojom::IntAttribute::kAriaNotificationInterruptDeprecated:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_ARIANOTIFICATIONINTERRUPTDEPRECATED;
    case ax::mojom::IntAttribute::kAriaNotificationPriorityDeprecated:
      return optimization_guide::proto::AXIntAttribute::
          AX_IA_ARIANOTIFICATIONPRIORITYDEPRECATED;
  }
}

optimization_guide::proto::AXFloatAttribute FloatAttributeToProto(
    ax::mojom::FloatAttribute attribute) {
  switch (attribute) {
    case ax::mojom::FloatAttribute::kNone:
      return optimization_guide::proto::AXFloatAttribute::AX_FA_NONE;
    case ax::mojom::FloatAttribute::kValueForRange:
      return optimization_guide::proto::AXFloatAttribute::AX_FA_VALUEFORRANGE;
    case ax::mojom::FloatAttribute::kMinValueForRange:
      return optimization_guide::proto::AXFloatAttribute::
          AX_FA_MINVALUEFORRANGE;
    case ax::mojom::FloatAttribute::kMaxValueForRange:
      return optimization_guide::proto::AXFloatAttribute::
          AX_FA_MAXVALUEFORRANGE;
    case ax::mojom::FloatAttribute::kStepValueForRange:
      return optimization_guide::proto::AXFloatAttribute::
          AX_FA_STEPVALUEFORRANGE;
    case ax::mojom::FloatAttribute::kFontSize:
      return optimization_guide::proto::AXFloatAttribute::AX_FA_FONTSIZE;
    case ax::mojom::FloatAttribute::kFontWeight:
      return optimization_guide::proto::AXFloatAttribute::AX_FA_FONTWEIGHT;
    case ax::mojom::FloatAttribute::kTextIndent:
      return optimization_guide::proto::AXFloatAttribute::AX_FA_TEXTINDENT;
    case ax::mojom::FloatAttribute::kChildTreeScale:
      return optimization_guide::proto::AXFloatAttribute::AX_FA_CHILDTREESCALE;
  }
}

optimization_guide::proto::AXBoolAttribute BoolAttributeToProto(
    ax::mojom::BoolAttribute attribute) {
  switch (attribute) {
    case ax::mojom::BoolAttribute::kNone:
      return optimization_guide::proto::AXBoolAttribute::AX_BA_NONE;
    case ax::mojom::BoolAttribute::kBusy:
      return optimization_guide::proto::AXBoolAttribute::AX_BA_BUSY;
    case ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot:
      return optimization_guide::proto::AXBoolAttribute::
          AX_BA_NONATOMICTEXTFIELDROOT;
    case ax::mojom::BoolAttribute::kContainerLiveAtomic:
      return optimization_guide::proto::AXBoolAttribute::
          AX_BA_CONTAINERLIVEATOMIC;
    case ax::mojom::BoolAttribute::kContainerLiveBusy:
      return optimization_guide::proto::AXBoolAttribute::
          AX_BA_CONTAINERLIVEBUSY;
    case ax::mojom::BoolAttribute::kLiveAtomic:
      return optimization_guide::proto::AXBoolAttribute::AX_BA_LIVEATOMIC;
    case ax::mojom::BoolAttribute::kModal:
      return optimization_guide::proto::AXBoolAttribute::AX_BA_MODAL;
    case ax::mojom::BoolAttribute::kUpdateLocationOnly:
      return optimization_guide::proto::AXBoolAttribute::
          AX_BA_UPDATELOCATIONONLY;
    case ax::mojom::BoolAttribute::kCanvasHasFallback:
      return optimization_guide::proto::AXBoolAttribute::
          AX_BA_CANVASHASFALLBACK;
    case ax::mojom::BoolAttribute::kScrollable:
      return optimization_guide::proto::AXBoolAttribute::AX_BA_SCROLLABLE;
    case ax::mojom::BoolAttribute::kClickable:
      return optimization_guide::proto::AXBoolAttribute::AX_BA_CLICKABLE;
    case ax::mojom::BoolAttribute::kClipsChildren:
      return optimization_guide::proto::AXBoolAttribute::AX_BA_CLIPSCHILDREN;
    case ax::mojom::BoolAttribute::kNotUserSelectableStyle:
      return optimization_guide::proto::AXBoolAttribute::
          AX_BA_NOTUSERSELECTABLESTYLE;
    case ax::mojom::BoolAttribute::kSelected:
      return optimization_guide::proto::AXBoolAttribute::AX_BA_SELECTED;
    case ax::mojom::BoolAttribute::kSelectedFromFocus:
      return optimization_guide::proto::AXBoolAttribute::
          AX_BA_SELECTEDFROMFOCUS;
    case ax::mojom::BoolAttribute::kSupportsTextLocation:
      return optimization_guide::proto::AXBoolAttribute::
          AX_BA_SUPPORTSTEXTLOCATION;
    case ax::mojom::BoolAttribute::kGrabbedDeprecated:
      return optimization_guide::proto::AXBoolAttribute::
          AX_BA_GRABBEDDEPRECATED;
    case ax::mojom::BoolAttribute::kIsLineBreakingObject:
      return optimization_guide::proto::AXBoolAttribute::
          AX_BA_ISLINEBREAKINGOBJECT;
    case ax::mojom::BoolAttribute::kIsPageBreakingObject:
      return optimization_guide::proto::AXBoolAttribute::
          AX_BA_ISPAGEBREAKINGOBJECT;
    case ax::mojom::BoolAttribute::kHasAriaAttribute:
      return optimization_guide::proto::AXBoolAttribute::AX_BA_HASARIAATTRIBUTE;
    case ax::mojom::BoolAttribute::kTouchPassthroughDeprecated:
      return optimization_guide::proto::AXBoolAttribute::
          AX_BA_TOUCHPASSTHROUGHDEPRECATED;
    case ax::mojom::BoolAttribute::kLongClickable:
      return optimization_guide::proto::AXBoolAttribute::AX_BA_LONGCLICKABLE;
    case ax::mojom::BoolAttribute::kHasHiddenOffscreenNodes:
      return optimization_guide::proto::AXBoolAttribute::
          AX_BA_HASHIDDENOFFSCREENNODES;
  }
}

optimization_guide::proto::AXIntListAttribute IntListAttributeToProto(
    ax::mojom::IntListAttribute attribute) {
  switch (attribute) {
    case ax::mojom::IntListAttribute::kNone:
      return optimization_guide::proto::AXIntListAttribute::AX_ILA_NONE;
    case ax::mojom::IntListAttribute::kIndirectChildIds:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_INDIRECTCHILDIDS;
    case ax::mojom::IntListAttribute::kControlsIds:
      return optimization_guide::proto::AXIntListAttribute::AX_ILA_CONTROLSIDS;
    case ax::mojom::IntListAttribute::kDetailsIds:
      return optimization_guide::proto::AXIntListAttribute::AX_ILA_DETAILSIDS;
    case ax::mojom::IntListAttribute::kDescribedbyIds:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_DESCRIBEDBYIDS;
    case ax::mojom::IntListAttribute::kErrormessageIds:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_ERRORMESSAGEIDS;
    case ax::mojom::IntListAttribute::kFlowtoIds:
      return optimization_guide::proto::AXIntListAttribute::AX_ILA_FLOWTOIDS;
    case ax::mojom::IntListAttribute::kLabelledbyIds:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_LABELLEDBYIDS;
    case ax::mojom::IntListAttribute::kRadioGroupIds:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_RADIOGROUPIDS;
    case ax::mojom::IntListAttribute::kMarkerTypes:
      return optimization_guide::proto::AXIntListAttribute::AX_ILA_MARKERTYPES;
    case ax::mojom::IntListAttribute::kMarkerStarts:
      return optimization_guide::proto::AXIntListAttribute::AX_ILA_MARKERSTARTS;
    case ax::mojom::IntListAttribute::kMarkerEnds:
      return optimization_guide::proto::AXIntListAttribute::AX_ILA_MARKERENDS;
    case ax::mojom::IntListAttribute::kHighlightTypes:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_HIGHLIGHTTYPES;
    case ax::mojom::IntListAttribute::kCaretBounds:
      return optimization_guide::proto::AXIntListAttribute::AX_ILA_CARETBOUNDS;
    case ax::mojom::IntListAttribute::kCharacterOffsets:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_CHARACTEROFFSETS;
    case ax::mojom::IntListAttribute::kLineStarts:
      return optimization_guide::proto::AXIntListAttribute::AX_ILA_LINESTARTS;
    case ax::mojom::IntListAttribute::kLineEnds:
      return optimization_guide::proto::AXIntListAttribute::AX_ILA_LINEENDS;
    case ax::mojom::IntListAttribute::kSentenceStarts:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_SENTENCESTARTS;
    case ax::mojom::IntListAttribute::kSentenceEnds:
      return optimization_guide::proto::AXIntListAttribute::AX_ILA_SENTENCEENDS;
    case ax::mojom::IntListAttribute::kWordStarts:
      return optimization_guide::proto::AXIntListAttribute::AX_ILA_WORDSTARTS;
    case ax::mojom::IntListAttribute::kWordEnds:
      return optimization_guide::proto::AXIntListAttribute::AX_ILA_WORDENDS;
    case ax::mojom::IntListAttribute::kCustomActionIds:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_CUSTOMACTIONIDS;
    case ax::mojom::IntListAttribute::kTextOperationStartAnchorIds:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_TEXTOPERATIONSTARTANCHORIDS;
    case ax::mojom::IntListAttribute::kTextOperationStartOffsets:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_TEXTOPERATIONSTARTOFFSETS;
    case ax::mojom::IntListAttribute::kTextOperationEndAnchorIds:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_TEXTOPERATIONENDANCHORIDS;
    case ax::mojom::IntListAttribute::kTextOperationEndOffsets:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_TEXTOPERATIONENDOFFSETS;
    case ax::mojom::IntListAttribute::kTextOperations:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_TEXTOPERATIONS;
    case ax::mojom::IntListAttribute::kAriaNotificationInterruptProperties:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_ARIANOTIFICATIONINTERRUPTPROPERTIES;
    case ax::mojom::IntListAttribute::kAriaNotificationPriorityProperties:
      return optimization_guide::proto::AXIntListAttribute::
          AX_ILA_ARIANOTIFICATIONPRIORITYPROPERTIES;
  }
}

optimization_guide::proto::AXStringListAttribute StringListAttributeToProto(
    ax::mojom::StringListAttribute attribute) {
  switch (attribute) {
    case ax::mojom::StringListAttribute::kNone:
      return optimization_guide::proto::AXStringListAttribute::AX_SLA_NONE;
    case ax::mojom::StringListAttribute::kAriaNotificationAnnouncements:
      return optimization_guide::proto::AXStringListAttribute::
          AX_SLA_ARIANOTIFICATIONANNOUNCEMENTS;
    case ax::mojom::StringListAttribute::kAriaNotificationIds:
      return optimization_guide::proto::AXStringListAttribute::
          AX_SLA_ARIANOTIFICATIONIDS;
    case ax::mojom::StringListAttribute::kCustomActionDescriptions:
      return optimization_guide::proto::AXStringListAttribute::
          AX_SLA_CUSTOMACTIONDESCRIPTIONS;
  }
}

void PopulateAXTreeData(const ui::AXTreeData& source,
                        optimization_guide::proto::AXTreeData* destination) {
  destination->set_doctype(source.doctype);
  destination->set_loaded(source.loaded);
  destination->set_loading_progress(source.loading_progress);
  destination->set_mimetype(source.mimetype);
  destination->set_title(source.title);
  destination->set_focus_id(source.focus_id);
  destination->set_sel_is_backward(source.sel_is_backward);
  destination->set_sel_anchor_object_id(source.sel_anchor_object_id);
  destination->set_sel_anchor_offset(source.sel_anchor_offset);
  destination->set_sel_anchor_affinity(
      TextAffinityToProto(source.sel_anchor_affinity));
  destination->set_sel_focus_object_id(source.sel_focus_object_id);
  destination->set_sel_focus_offset(source.sel_focus_offset);
  destination->set_sel_focus_affinity(
      TextAffinityToProto(source.sel_focus_affinity));
  destination->set_root_scroller_id(source.root_scroller_id);
  for (const auto& metadata : source.metadata) {
    *destination->add_metadata() = metadata;
  }
}

void PopulateAXRelativeBounds(
    const ui::AXRelativeBounds& source,
    optimization_guide::proto::AXRelativeBounds* destination) {
  destination->set_offset_container_id(source.offset_container_id);
  destination->set_x(source.bounds.x());
  destination->set_y(source.bounds.y());
  destination->set_width(source.bounds.width());
  destination->set_height(source.bounds.height());
  if (source.transform) {
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        destination->add_transform(source.transform->rc(r, c));
      }
    }
  }
}

void PopulateAXNode(const ui::AXNodeData& source,
                    optimization_guide::proto::AXNodeData* destination) {
  destination->set_id(source.id);
  destination->set_role(RoleToProto(source.role));
  destination->set_state(source.state);
  destination->set_actions(source.actions);

  for (const auto& attribute : source.string_attributes) {
    auto* destination_attribute = destination->add_attributes();
    destination_attribute->set_string_type(
        StringAttributeToProto(attribute.first));
    destination_attribute->set_string_value(attribute.second);
  }

  for (const auto& attribute : source.int_attributes) {
    auto* destination_attribute = destination->add_attributes();
    destination_attribute->set_int_type(IntAttributeToProto(attribute.first));
    destination_attribute->set_int_value(attribute.second);
  }

  for (const auto& attribute : source.float_attributes) {
    auto* destination_attribute = destination->add_attributes();
    destination_attribute->set_float_type(
        FloatAttributeToProto(attribute.first));
    destination_attribute->set_float_value(attribute.second);
  }

  for (const auto& attribute : source.bool_attributes) {
    auto* destination_attribute = destination->add_attributes();
    destination_attribute->set_bool_type(BoolAttributeToProto(attribute.first));
    destination_attribute->set_bool_value(attribute.second);
  }

  for (const auto& attribute : source.intlist_attributes) {
    auto* destination_attribute = destination->add_attributes();
    destination_attribute->set_intlist_type(
        IntListAttributeToProto(attribute.first));
    auto* int_list_value = destination_attribute->mutable_int_list_value();
    for (int value : attribute.second) {
      int_list_value->add_value(value);
    }
  }

  for (const auto& attribute : source.stringlist_attributes) {
    auto* destination_attribute = destination->add_attributes();
    destination_attribute->set_stringlist_type(
        StringListAttributeToProto(attribute.first));
    auto* string_list_value =
        destination_attribute->mutable_string_list_value();
    for (const std::string& value : attribute.second) {
      string_list_value->add_value(value);
    }
  }

  for (const auto& attribute : source.html_attributes) {
    auto* destination_attribute = destination->add_attributes();
    destination_attribute->set_html_attribute_name(attribute.first);
    destination_attribute->set_html_attribute_value(attribute.second);
  }

  for (const auto& child_id : source.child_ids) {
    destination->add_child_ids(child_id);
  }

  PopulateAXRelativeBounds(source.relative_bounds,
                           destination->mutable_relative_bounds());
}

}  // namespace

proto::Any AnyWrapProto(const google::protobuf::MessageLite& m) {
  optimization_guide::proto::Any any;
  any.set_type_url("type.googleapis.com/" + m.GetTypeName());
  m.SerializeToString(any.mutable_value());
  return any;
}

void PopulateAXTreeUpdateProto(
    const ui::AXTreeUpdate& source,
    optimization_guide::proto::AXTreeUpdate* destination) {
  destination->set_root_id(source.root_id);
  if (source.has_tree_data) {
    PopulateAXTreeData(source.tree_data, destination->mutable_tree_data());
  }

  for (auto& node : source.nodes) {
    PopulateAXNode(node, destination->add_nodes());
  }
}

}  // namespace optimization_guide
