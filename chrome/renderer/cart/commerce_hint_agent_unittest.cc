// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/cart/commerce_hint_agent.h"

#include <string_view>

#include "base/cfi_buildflags.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/commerce/core/commerce_heuristics_data.h"
#include "components/commerce/core/commerce_heuristics_data_metrics_helper.h"
#if BUILDFLAG(IS_ANDROID)
#include "components/commerce/core/commerce_feature_list.h"
#else
#include "components/search/ntp_features.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr int kTestIterations = 1000;

// clang-format off
const char* kAddToCart[] = {
  "submit.add-to-cart: Submit",
  "gp/add-to-cart/json/",
  "-addtocart&",
  "?add-to-cart=1",
  "action: peake_add_to_basket",
  "action: woosea_addtocart_details",
  "ajax=add_to_cart",
  "action: woocommerce_add_to_cart",
  "add-to-cart: 123",
  "queryString=submit.addToCart",
  "queryString=submit.AddToCart",
  "&link=Add%20to%20Cart",
  "api_call:shopping_bag_add_to_bag", // nordstrom.com
  "\"cart_type\":\"REGULAR\"", // target.com
  "cnc/checkout/cartItems/addItemToCart", // kohls.com
  "\"event\":\"product_added_to_cart\"", // staples.com
  "checkout/basket/add_and_show", // wayfair.com
  "isQuickAddToCartButton=true", // costco.com
};

const char* kAddToCartWithDOMBasedHeuristics[] = {
  "{\"productId\":\"123\",\"quantity\":14}",
  "{\"productId\":\"123\",\"quantity\":\"151\"}",
  "{\"Quantity\":\"2\"}",
  "{\"cart_quantity\":\"23\"}",
  "{\"product_quantity\":76}",
};

const char* kNotAddToCart[] = {
  "misadd-to-cart",
  "add_to_cartoon",
  "_add_to_basketball",
  "cart/address",
  "golfcart/add",
};

const char* kNotAddToCartWithDOMBasedHeuristics[] = {
  "{\"productName\":\"quantity_calculator\"}",
  "quantity=1",
  "{\"product_quantity\":ab}",
  "{\"quality\":\"2\"}",
};

const char* kVisitCart[] = {
    // Real cart URLs.
    "https://www.brownells.com/aspx/Store/Cart.aspx",
    "https://www.carid.com/cart.php",
    "https://www.chegg.com/shoppingcart",
    "https://www.target.com/co-cart",
    "https://beastacademy.com/checkout/cart",
    "https://cart.ebay.com/",
    "https://cart.ebay.com/sc/view",
    "https://cart.godaddy.com/",
    "https://cart.godaddy.com/Basket.aspx",
    "https://cart.godaddy.com/basket.aspx",
    "https://cart.godaddy.com/go/checkout",
    "https://cart.godaddy.com/upp/vcart",
    "https://cart.payments.ebay.com/sc/add",
    "https://es-store.usps.com/store/cart/cart.jsp",
    "https://poshmark.com/bundles/shop",
    "https://secure-athleta.gap.com/shopping-bag",
    "https://secure-bananarepublic.gap.com/shopping-bag",
    "https://secure-oldnavy.gap.com/buy/shopping_bag.do",
    "https://secure-oldnavy.gap.com/shopping-bag",
    "https://secure-www.gap.com/shopping-bag",
    "https://secure.newegg.com/Shop/Cart",
    "https://secure.newegg.com/shop/cart",
    "https://secure2.homedepot.com/mycart/home",
    "https://shop.advanceautoparts.com/web/OrderItemDisplay",
    "https://shop.lululemon.com/shop/mybag",
    "https://smile.amazon.co.uk/gp/cart/view.html",
    "https://smile.amazon.com/gp/aw/c?ref_=navm_hdr_cart",
    "https://smile.amazon.com/gp/cart/view.html",
    "https://store.bricklink.com/v2/globalcart.page",
    "https://store.steampowered.com/cart",
    "https://store.steampowered.com/cart/",
    "https://store.usps.com/store/cart/cart.jsp",
    "https://training.atlassian.com/auth/cart",
    "https://www.abebooks.com/servlet/ShopBasketPL",
    "https://www.abebooks.com/servlet/ShoppingBasket",
    "https://www.academy.com/shop/cart",
    "https://www.acehardware.com/cart",
    "https://www.adorama.com/Als.Mvc/CartView",
    "https://www.adorama.com/als.mvc/cartview",
    "https://www.ae.com/us/en/cart",
    "https://www.altardstate.com/cart/",
    "https://www.amazon.co.uk/gp/aws/cart/add.html",
    "https://www.amazon.co.uk/gp/cart/view.html",
    "https://www.amazon.co.uk/gp/cart/view.html/ref=chk_logo_return_to_cart",
    "https://www.amazon.co.uk/gp/cart/view.html/ref=lh_cart",
    "https://www.amazon.com/-/es/gp/cart/view.html",
    "https://www.amazon.com/gp/aw/c?ref_=navm_hdr_cart",
    "https://www.amazon.com/gp/aws/cart/add.html",
    "https://www.amazon.com/gp/cart/view.html",
    "https://www.amazon.com/gp/cart/view.html/ref=lh_cart",
    "https://www.amazon.com/gp/cart/view.html/ref=lh_cart_vc_btn",
    "https://www.anthropologie.com/cart",
    "https://www.apple.com/shop/bag",
    "https://www.apple.com/us-hed/shop/bag",
    "https://www.apple.com/us/shop/goto/bag",
    "https://www.apple.com/us_epp_805199/shop/bag",
    "https://www.atlassian.com/purchase/cart",
    "https://www.att.com/buy/cart",
    "https://www.att.com/buy/checkout/cartview",
    "https://www.backcountry.com/Store/cart/cart.jsp",
    "https://www.basspro.com/shop/AjaxOrderItemDisplayView",
    "https://www.bathandbodyworks.com/cart",
    "https://www.bedbathandbeyond.com/store/cart",
    "https://www.belk.com/shopping-bag/",
    "https://www.bestbuy.com/cart",
    "https://www.bhphotovideo.com/c/find/cart.jsp",
    "https://www.bhphotovideo.com/find/cart.jsp",
    "https://www.bhphotovideo.com/find/cart.jsp/mode/edu",
    "https://www.bloomingdales.com/my-bag",
    "https://www.boostmobile.com/cart.html",
    "https://www.bricklink.com/v2/globalcart.page",
    "https://www.brownells.com/aspx/Store/Cart.aspx",
    "https://www.brownells.com/aspx/store/cart.aspx",
    "https://www.buybuybaby.com/store/cart",
    "https://www.carid.com/cart.php",
    "https://www.chegg.com/shoppingcart",
    "https://www.chegg.com/shoppingcart/",
    "https://www.containerstore.com/cart/list.htm",
    "https://www.costco.com/CheckoutCartDisplayView",
    "https://www.costco.com/CheckoutCartView",
    "https://www.crateandbarrel.com/Checkout/Cart",
    "https://www.crateandbarrel.com/checkout/cart",
    "https://www.dickssportinggoods.com/OrderItemDisplay",
    "https://www.dillards.com/webapp/wcs/stores/servlet/OrderItemDisplay",
    "https://www.dsw.com/en/us/shopping-bag",
    "https://www.electronicexpress.com/cart",
    "https://www.etsy.com/cart",
    "https://www.etsy.com/cart/",
    "https://www.etsy.com/cart/listing.php",
    "https://www.etsy.com/cart/listing.php",
    "https://www.eyebuydirect.com/cart",
    "https://www.fingerhut.com/cart/index",
    "https://www.finishline.com/store/cart/cart.jsp",
    "https://www.freepeople.com/cart",
    "https://www.freepeople.com/cart/",
    "https://www.gamestop.com/cart",
    "https://www.gamestop.com/cart/",
    "https://www.groupon.com/cart",
    "https://www.groupon.com/checkout/cart",
    "https://www.harborfreight.com/checkout/cart",
    "https://www.hmhco.com/hmhstorefront/cart",
    "https://www.homedepot.com/mycart/home",
    "https://www.homesquare.com/Checkout/Cart.aspx",
    "https://www.hottopic.com/cart",
    "https://www.hsn.com/checkout/bag",
    "https://www.ikea.com/us/en/shoppingcart/",
    "https://www.jcpenney.com/cart",
    "https://www.jcrew.com/checkout/cart",
    "https://www.joann.com/cart",
    "https://www.kohls.com/checkout/shopping_cart.jsp",
    "https://www.landsend.com/shopping-bag",
    "https://www.landsend.com/shopping-bag/",
    "https://www.llbean.com/webapp/wcs/stores/servlet/LLBShoppingCartDisplay",
    "https://www.lowes.com/cart",
    "https://www.lulus.com/checkout/bag",
    "https://www.macys.com/my-bag",
    "https://www.microsoft.com/en-US/store/cart",
    "https://www.microsoft.com/en-us/store/buy/cart",
    "https://www.microsoft.com/en-us/store/cart",
    "https://www.midwayusa.com/cart",
    "https://www.midwayusa.com/cart/",
    "https://www.neimanmarcus.com/checkout/cart.jsp",
    "https://www.nike.com/cart",
    "https://www.nordstrom.com/shopping-bag",
    "https://www.officedepot.com/cart/shoppingCart.do",
    "https://www.opticsplanet.com/checkout/cart",
    "https://www.overstock.com/cart",
    ("https://www.pacsun.com/on/demandware.store/Sites-pacsun-Site/default/"
     "Cart-Show"),
    "https://www.petsmart.com/cart/",
    "https://www.pier1.com/cart",
    "https://www.pokemoncenter.com/cart",
    "https://www.potterybarn.com/shoppingcart/",
    "https://www.qvc.com/checkout/cart.html",
    "https://www.redbubble.com/cart",
    "https://www.rei.com/ShoppingCart",
    "https://www.revolve.com/r/ShoppingBag.jsp",
    "https://www.rockauto.com/en/cart",
    "https://www.rockauto.com/en/cart/",
    "https://www.rockauto.com/en/cart/checkout",
    "https://www.saksfifthavenue.com/cart",
    "https://www.samsclub.com/cart",
    "https://www.samsclub.com/sams/cart/cart.jsp",
    "https://www.sephora.com/basket",
    "https://www.shutterfly.com/cart",
    "https://www.shutterfly.com/cart/",
    "https://www.staples.com/cc/mmx/cart",
    "https://www.sweetwater.com/store/cart.php",
    "https://www.talbots.com/cart",
    "https://www.target.com/co-cart",
    "https://www.target.com/co-cart",
    "https://www.teacherspayteachers.com/Cart",
    "https://www.teacherspayteachers.com/Cart/Checkout",
    "https://www.therealreal.com/cart",
    "https://www.tractorsupply.com/TSCShoppingCartView",
    "https://www.ulta.com/bag",
    "https://www.ulta.com/bag/empty",
    "https://www.ulta.com/bag/login",
    "https://www.underarmour.com/en-us/cart",
    "https://www.urbanoutfitters.com/cart",
    "https://www.vitalsource.com/cart",
    "https://www.walgreens.com/cart/view-ui",
    "https://www.walmart.com/cart",
    "https://www.walmart.com/cart/",
    "https://www.wayfair.com/session/public/basket.php",
    "https://www.wayfair.com/v/checkout/basket/add_and_show",
    "https://www.wayfair.com/v/checkout/basket/show",
    "https://www.webstaurantstore.com/viewcart.cfm",
    "https://www.weightwatchers.com/us/shop/checkout/cart",
    "https://www.westelm.com/shoppingcart/",
    "https://www.wiley.com/en-us/cart",
    "https://www.wish.com/cart"
    "https://www.williams-sonoma.com/shoppingcart/",
    "https://www.zappos.com/cart",
    "https://www.zazzle.com/co/cart",
    "https://www.zennioptical.com/shoppingCart",
    "https://www2.hm.com/en_gb/cart",
    "https://www2.hm.com/en_us/cart",
    // Example cart URLs.
    "http://example.com/us/cart/",
    "http://example.com/cart/",
    "https://example.com/cart",
    "http://example.com/cart",
    "http://example.com/cart?param",
    "http://example.com/cart#anchor",
    "http://example.com/cart?param=value#anchor",
    "https://www.example.com/cart/list.htm",
    "https://cart.example.com",
    "https://www.example.com/my-cart",
    "https://www.example.com/CartView/",
    "https://www.example.com/checkout/cart",
    "https://www.example.com/CheckoutCartView/",
    "https://example.com/bundles/shop",
    "https://www.example.com/AjaxOrderItemDisplay",
    "https://www.example.com/OrderItemDisplayView.jsp",
    "https://www.example.com/cart-show",
};

const char* kNotVisitCart[] = {
    // Real non-cart URLs.
    "https://www.rockauto.com/xx/cart/",
    "https://api.bestbuy.com/click/-/6429440/cart",
    "https://api.bestbuy.com/click/-/6429442/cart",
    "https://business.landsend.com/checkout/cart",
    "https://business.officedepot.com/cart/checkout.do",
    "https://business.officedepot.com/cart/shoppingCart.do",
    "https://business.officedepot.com/cart/updateRouter.do",
    "https://cart.ebay.com/api/xo",
    "https://cns.usps.com/shippingCart",
    "https://cns.usps.com/shippingCart.shtml",
    "https://ecommerce2.apple.com/asb2bstorefront/asb2b/en/USD/cart",
    "https://factory.jcrew.com/checkout/cart",
    "https://m.llbean.com/webapp/wcs/stores/servlet/LLBShoppingCartDisplay",
    "https://photo.samsclub.com/cart/items",
    "https://photo.walgreens.com/cart/confirmation",
    "https://photo.walgreens.com/cart/review",
    "https://photo.walgreens.com/cart/shipping",
    "https://photo.walgreens.com/cart/shoppingcart",
    "https://photos3.walmart.com/shoppingcart",
    "https://poshmark.com/bundles/sell",
    "https://poshmark.com/category/Women-Bags",
    "https://poshmark.com/category/Women-Bags-Crossbody_Bags",
    "https://poshmark.com/showroom/"
    "Louis-Vuitton-Handbags-1b6f536521bf8d6e0a155371",
    "https://secure.newegg.com/Shopping/AddToCart.aspx",
    "https://secure.newegg.com/Shopping/AddtoCart.aspx",
    "https://secure.newegg.com/Shopping/EmailCart.aspx",
    "https://shop.lululemon.com/c/bags/_/N-1z0xcuuZ8rd",
    "https://shop.lululemon.com/c/bags/_/N-8rd",
    "https://shop.lululemon.com/p/bags/All-Night-Festival-Bag-Micro/_/"
    "prod9960617",
    "https://shop.lululemon.com/p/bags/Everywhere-Belt-Bag/_/prod8900747",
    "https://store.steampowered.com/app/1220140/Cartel_Tycoon/",
    "https://store.steampowered.com/itemcart/checkout/",
    "https://store.steampowered.com/points/shop/c/itembundles",
    "https://store.usps.com/store/product/shipping-supplies/"
    "readypost-15l-x-12w-x-10h-mailing-cartons-P_843061",
    "https://www.academy.com/shop/browse/camping--outdoors/"
    "sleeping-bags-and-bedding/airbeds--sleeping-pads",
    "https://www.academy.com/shop/browse/outdoors/camping--outdoors/"
    "sleeping-bags-airbeds-cots/airbeds--sleeping-pads",
    "https://www.academy.com/shop/browse/sports/basketball/basketball-hoops",
    "https://www.academy.com/shop/browse/sports/boxing--mma/"
    "boxing-mma-punching-bags/heavy-bags",
    "https://www.acehardware.com/cart/checkout",
    "https://www.acehardware.com/departments/home-and-decor/"
    "trash-and-recycling/garbage-cans-and-recycling-bins",
    "https://www.acehardware.com/departments/lawn-and-garden/gardening-tools/"
    "wheelbarrows-carts-and-hand-trucks",
    "https://www.acehardware.com/departments/lawn-and-garden/gardening-tools/"
    "wheelbarrows-carts-and-hand-trucks/7331739",
    "https://www.adorama.com/l/Photography/Photography-Bags-and-Cases",
    "https://www.ae.com/us/en/c/aerie/accessories-shoes/bags/cat6460064",
    "https://www.ae.com/us/en/c/women/accessories-socks/belts-bags/cat1070004",
    "https://www.ae.com/us/en/p/women/high-waisted-shorts/mom-shorts/"
    "ae-paperbag-denim-mom-shorts/0338_6267_489",
    "https://www.ae.com/us/en/p/women/jogger-pants/jogger-pants/"
    "ae-paperbag-jogger-pant/0322_4413_309",
    "https://www.altardstate.com/as/all-accessories/bags-keychains/",
    "https://www.anthropologie.com/bags",
    "https://www.anthropologie.com/shop/julien-leather-tote-bag",
    "https://www.anthropologie.com/shop/liberty-crossbody-bag",
    "https://www.anthropologie.com/shop/luna-slouchy-crossbody-bag",
    "https://www.att.com/buy/bundles",
    "https://www.att.com/buy/bundles/",
    "https://www.att.com/buy/bundles/hsiaplans",
    "https://www.backcountry.com/backcountry-double-ski-snowboard-rolling-bag",
    "https://www.backcountry.com/down-sleeping-bags",
    "https://www.backcountry.com/patagonia-baggies-shorts-mens",
    "https://www.backcountry.com/sleeping-bags",
    "https://www.basspro.com/shop/en/sleeping-bags",
    "https://www.basspro.com/shop/en/"
    "spring-fishing-classic-sale-tackle-boxes-and-bags",
    "https://www.basspro.com/shop/en/tackle-bags",
    "https://www.basspro.com/shop/en/tackle-boxes-bags",
    "https://www.bathandbodyworks.com/c/gifts/gift-basket-stuffers",
    "https://www.bathandbodyworks.com/on/demandware.store/"
    "Sites-BathAndBodyWorks-Site/en_US/Cart-AddProduct",
    "https://www.bathandbodyworks.com/on/demandware.store/"
    "Sites-BathAndBodyWorks-Site/en_US/Cart-AddToWishlist",
    "https://www.bathandbodyworks.com/on/demandware.store/"
    "Sites-BathAndBodyWorks-Site/en_US/Cart-ContinueShopping",
    "https://www.bedbathandbeyond.com/store/category/bedding/bed-in-a-bag/"
    "16078/",
    "https://www.bedbathandbeyond.com/store/category/bedding/bed-in-a-bag/"
    "16078/_full-queen_queen/"
    "dmlzdWFsVmFyaWFudC5ub252aXN1YWxWYXJpYW50LlNLVV9GT1JfU1dBVENILlNLVV9TSVpFOi"
    "JmdWxsL3F1ZWVuInwicXVlZW4i",
    "https://www.bedbathandbeyond.com/store/category/storage-cleaning/"
    "storage-organization/drawers-carts/12642/",
    "https://www.bedbathandbeyond.com/store/category/storage-cleaning/"
    "storage-organization/storage-bins-baskets/12208/",
    "https://www.belk.com/bed-bath/bedding/bed-in-a-bag/",
    "https://www.belk.com/handbags/",
    "https://www.belk.com/handbags/purses-handbags/",
    "https://www.belk.com/handbags/purses-handbags/crossbody-bags/",
    "https://www.bestbuy.com/site/combo/washer-dryer-bundles/"
    "af7c2f64-5faa-4a9e-a079-7246f70db472",
    "https://www.bestbuy.com/site/washers-dryers/washer-dryer-bundles/"
    "pcmcat303000050004.c",
    "https://www.bhphotovideo.com/c/browse/Bags-Cases-Carrying-Equipment/ci/"
    "167/N/4075788798",
    "https://www.bhphotovideo.com/c/buy/Shoulder-Gadget-Bags/ci/174",
    "https://www.bloomingdales.com/shop/handbags",
    "https://www.bloomingdales.com/shop/handbags/clutches-evening-bags",
    "https://www.bloomingdales.com/shop/sale/handbags-purses",
    "https://www.bloomingdales.com/shop/tory-burch/handbags-wallets",
    "https://www.boostmobile.com/cart",
    "https://www.buybuybaby.com/store/category/gear-travel/diaper-bags/30507/",
    "https://www.buybuybaby.com/store/category/gear-travel/diaper-bags/"
    "diaper-backpacks/32034/",
    "https://www.buybuybaby.com/store/product/"
    "carter-39-s-by-davinci-adrian-swivel-glider-with-ottoman-in-performance-"
    "fabric/5354803",
    "https://www.buybuybaby.com/store/registry/FreeGoodyBag",
    "https://www.chegg.com/homework-help/"
    "mauro-products-distributes-single-product-woven-basket-whose-chapter-6-"
    "problem-7be-solution-9780077386214-exc",
    "https://www.chegg.com/homework-help/questions-and-answers/"
    "example-1010-p-252-suppose-mountain-climber-instead-drags-bag-supplies-"
    "slope-increasing-sp-q31806760",
    "https://www.containerstore.com/s/laundry-cleaning/hampers-baskets/12",
    "https://www.containerstore.com/s/storage/decorative-bins-baskets/12",
    "https://www.containerstore.com/s/storage/plastic-bins-baskets/12",
    "https://www.containerstore.com/s/storage/plastic-bins-baskets/"
    "white-nordic-storage-baskets-with-handles/12d",
    "https://www.costco.com/ManageShoppingCartCmd",
    "https://www.costco.com/handbags-wallets.html",
    "https://www.costco.com/logon-instacart",
    "https://www.crateandbarrel.com/decorating-and-accessories/baskets/1",
    "https://www.crateandbarrel.com/furniture/bar-cabinets-and-carts/1",
    "https://www.crateandbarrel.com/furniture/filing-cabinets-and-carts/1",
    "https://www.dickssportinggoods.com/c/basketball-gear",
    "https://www.dickssportinggoods.com/f/basketball-shoes-for-men",
    "https://www.dickssportinggoods.com/f/golf-bags-accessories-1",
    "https://www.dickssportinggoods.com/p/"
    "jordan-air-jordan-1-mid-basketball-shoes-19nikmrjrdn1mdblknke/"
    "19nikmrjrdn1mdblknke",
    "https://www.dillards.com/brand/Michael+Kors/handbags",
    "https://www.dillards.com/c/handbags",
    "https://www.dillards.com/c/handbags-cross-body-bags",
    "https://www.dillards.com/c/sale-clearance/handbags",
    "https://www.dsw.com/en/us/category/womens-clearance-handbags/"
    "N-1z141jrZ1z141ilZ1z141cp",
    "https://www.dsw.com/en/us/category/womens-crossbody-handbags/"
    "N-1z141jrZ1z128u1Z1z141cp",
    "https://www.dsw.com/en/us/category/womens-handbags/N-1z141jrZ1z141cp",
    "https://www.dsw.com/en/us/category/womens-leather-and-nubuck-handbags/"
    "N-1z141jrZ1z12bixZ1z12b4gZ1z12b7gZ1z141cp",
    "https://www.etsy.com/c/bags-and-purses/handbags",
    "https://www.etsy.com/c/bags-and-purses/handbags/shoulder-bags",
    "https://www.eyebuydirect.com/sunglasses/frames/cartel-tortoise-m-19614",
    "https://www.fingerhut.com/cart/justAdded",
    "https://www.fingerhut.com/search/Clothing/Handbags",
    "https://www.fingerhut.com/search/Clothing/Handbags/2126.uts",
    "https://www.fingerhut.com/search/Clothing/Luggage%20&%20Travel%20Bags",
    "https://www.finishline.com/store/cart/cartSlide.jsp",
    "https://www.finishline.com/store/men/shoes/basketball/_/N-3z3fil",
    "https://www.finishline.com/store/product/"
    "mens-air-jordan-6-rings-basketball-shoes/prod739613",
    "https://www.finishline.com/store/product/"
    "nike-zoom-freak-2-basketball-shoes/prod2800331",
    "https://www.freepeople.com/bags/",
    "https://www.freepeople.com/shop/carter-sweater-set/",
    "https://www.freepeople.com/shop/hudson-sling-bag/",
    "https://www.gamestop.com/clothing/bags-travel/backpacks",
    "https://www.gamestop.com/toys-collectibles/toys/blind-bags",
    "https://www.gamestop.com/toys-collectibles/toys/plush/products/"
    "squishmallows-plush-bag-clip-assortment/11108921.html",
    "https://www.groupon.com/coupons/carters",
    "https://www.groupon.com/coupons/instacart",
    "https://www.groupon.com/deals/"
    "gg-mp-oled-fingertip-pulse-oximeter-finger-blood-oxygen-spo2-pr-heart-"
    "rate-monitor-bag",
    "https://www.harborfreight.com/material-handling/"
    "hand-trucks-carts-dollies.html",
    "https://www.harborfreight.com/tool-storage-organization/"
    "tool-boxes-bags-belts.html",
    "https://www.harborfreight.com/tool-storage-organization/tool-storage/"
    "tool-carts.html",
    "https://www.harborfreight.com/tool-storage-organization/"
    "yukon-tool-storage/tool-carts-cabinets.html",
    "https://www.homedepot.com/b/Appliances-Garbage-Disposals/N-5yc1vZc3no",
    "https://www.homedepot.com/p/"
    "GE-4-2-cu-ft-White-Top-Load-Washing-Machine-with-Stainless-Steel-Basket-"
    "GTW335ASNWW/308653937",
    "https://www.homedepot.com/p/"
    "GE-4-5-cu-ft-High-Efficiency-White-Top-Load-Washing-Machine-with-"
    "Stainless-Steel-Basket-GTW465ASNWW/308653940",
    "https://www.homedepot.com/p/Vigoro-2-cu-ft-Bagged-Brown-Mulch-52050196/"
    "205606287",
    "https://www.hottopic.com/accessories/bags/",
    "https://www.hottopic.com/accessories/bags/backpacks/",
    "https://www.hottopic.com/accessories/bags/wallets/",
    "https://www.hottopic.com/on/demandware.store/Sites-hottopic-Site/default/"
    "Cart-AddToWishlist",
    "https://www.hsn.com/shop/clearance-handbags-and-wallets-for-women/"
    "fa0402-4525",
    "https://www.hsn.com/shop/handbags-and-wallets-for-women/fa0402",
    "https://www.hsn.com/shop/hands-free-handbags/20986",
    "https://www.hsn.com/shop/patricia-nash-handbags-and-wallets-for-women/"
    "fa0402-16311",
    "https://www.ikea.com/us/en/cat/baskets-16201/",
    "https://www.ikea.com/us/en/cat/kitchen-islands-carts-10471/",
    "https://www.ikea.com/us/en/cat/kitchen-islands-carts-fu005/",
    "https://www.ikea.com/us/en/cat/storage-boxes-baskets-10550/",
    "https://www.jcpenney.com/cart/signin",
    "https://www.jcpenney.com/g/purses-accessories/crossbody-bags",
    "https://www.jcpenney.com/g/purses-accessories/view-all-handbags-wallets",
    "https://www.jcpenney.com/jsp/cart/viewShoppingBag.jsp",
    "https://www.jcrew.com/c/mens_category/bags",
    "https://www.jcrew.com/c/womens_category/handbags",
    "https://www.jcrew.com/checkout2/shoppingbag.jsp",
    "https://www.joann.com/home-decor-and-holiday/"
    "storage-and-organization-decor/baskets/",
    "https://www.joann.com/iris-usa-6-drawer-medium-cart/7226947.html",
    "https://www.joann.com/sewing/sewing-baskets-and-pin-cushions/",
    "https://www.joann.com/we-r-memory-keepers-a-la-cart/zprd_18086751a.html",
    "https://www.kohls.com/catalog/"
    "womens-totes-handbags-purses-accessories.jsp",
    "https://www.kohls.com/ecom/kohls-smartcart-kcash.html",
    "https://www.kohls.com/product/prd-1369234/"
    "shark-navigator-lift-away-deluxe-professional-bagless-vacuum.jsp",
    "https://www.kohls.com/product/prd-3698449/"
    "mens-nike-dri-fit-icon-basketball-shorts.jsp",
    "https://www.landsend.com/products/open-or-zip-top-natural-canvas-tote-bag/"
    "id_299717",
    "https://www.landsend.com/shop/bags-travel-sale/S-xfd-ytq-xec",
    "https://www.llbean.com/llb/shop/webapp/wcs/stores/servlet/"
    "LLBShoppingCartDisplay",
    "https://www.lowes.com/pl/Bagged-mulch-Mulch-Landscaping-Lawn-garden/"
    "4294612786",
    "https://www.lowes.com/pl/"
    "Kitchen-islands-carts-Dining-kitchen-furniture-Furniture-Home-decor/"
    "1323549633",
    "https://www.lowes.com/pl/"
    "Plastic-storage-totes-Baskets-storage-containers-Storage-organization/"
    "4294713243",
    "https://www.lowes.com/pl/"
    "Wheelbarrows-Wheelbarrows-yard-carts-Outdoor-tools-equipment-Outdoors/"
    "3394587996",
    "https://www.lulus.com/categories/39_598/vegan-handbags-and-purses.html",
    "https://www.lulus.com/categories/99_39/handbags-and-purses.html",
    "https://www.lulus.com/products/weekend-traveler-black-and-cognac-tote-bag/"
    "1029302.html",
    "https://www.lulus.com/products/weekend-traveler-cognac-tote-bag/"
    "1029322.html",
    "https://www.macys.com/shop/bed-bath/bed-in-a-bag",
    "https://www.macys.com/shop/featured/michael-kors-handbags",
    "https://www.macys.com/shop/handbags-accessories",
    "https://www.macys.com/shop/sale/Business_category/"
    "Handbags%20%26%20Accessories%7CWomen%7CWomen%27s%20Shoes",
    "https://www.microsoft.com/en-us/store/collections/surfacebagsandsleeves/"
    "pc",
    "https://www.microsoft.com/en-us/store/collections/surfacebundles",
    "https://www.midwayusa.com/backpacks-and-bags/br",
    "https://www.midwayusa.com/range-bags/br",
    "https://www.midwayusa.com/shooting-rests-and-bags/br",
    "https://www.neimanmarcus.com/c/handbags-all-handbags-cat46860739",
    "https://www.neimanmarcus.com/c/handbags-cat13030735",
    "https://www.neimanmarcus.com/c/sale-all-sale-women-handbags-cat74120753",
    "https://www.neimanmarcus.com/c/sale-women-handbags-cat46520737",
    "https://www.nike.com/w/mens-bags-and-backpacks-9xy71znik1",
    "https://www.nike.com/w/mens-basketball-shoes-3glsmznik1zy7ok",
    "https://www.nike.com/w/"
    "mens-nike-by-you-basketball-shoes-3glsmz6ealhznik1zy7ok",
    "https://www.nike.com/w/womens-basketball-shoes-3glsmz5e1x6zy7ok",
    "https://www.nordstrom.com/browse/designer/women/handbags",
    "https://www.nordstrom.com/browse/sale/women/handbags",
    "https://www.nordstrom.com/browse/women/handbags",
    "https://www.nordstrom.com/browse/women/handbags/crossbody",
    "https://www.officedepot.com/cart/checkout.do",
    "https://www.opticsplanet.com/cart-empty",
    "https://www.overstock.com/Bedding-Bath/Bed-in-a-Bag/29942/cat.html",
    "https://www.overstock.com/Home-Garden/Kitchen-Carts/1996/subcat.html",
    "https://www.overstock.com/Home-Garden/"
    "Malia-Outdoor-Standing-Wicker-Basket-Chair-with-Cushion-by-Christopher-"
    "Knight-Home/29874486/product.html",
    "https://www.overstock.com/Home-Garden/Simple-Living-Georgia-Kitchen-Cart/"
    "30992405/product.html",
    "https://www.pacsun.com/mens/backpacks-bags/",
    "https://www.pacsun.com/on/demandware.store/Sites-pacsun-Site/default/"
    "Cart-ContinueShopping",
    "https://www.pacsun.com/womens/accessories/handbags/",
    "https://www.pacsun.com/womens/jeans/baggy/",
    "https://www.petsmart.com/cart-checkout/",
    "https://www.petsmart.com/fish/filters-and-pumps/filter-media/"
    "top-fin-ef-s-element-aquarium-filter-cartridges---12pk-57278.html",
    "https://www.petsmart.com/on/demandware.store/Sites-PetSmart-Site/default/"
    "Cart-ContinueShopping",
    "https://www.petsmart.com/on/demandware.store/Sites-PetSmart-Site/default/"
    "Paypal-StartExpressCheckoutFromCartFlow",
    "https://www.pier1.com/cart/add",
    "https://www.pier1.com/cart/change",
    "https://www.pier1.com/products/sisal-bunny-girl-with-basket-15h",
    "https://www.pier1.com/products/twig-basket-with-bunny",
    "https://www.pokemoncenter.com/category/bags-and-totes",
    "https://www.potterybarn.com/products/beachcomber-basket-collection/",
    "https://www.potterybarn.com/products/seagrass-basket-collection-havana/",
    "https://www.potterybarn.com/products/seagrass-basket-collection-savannah/",
    "https://www.potterybarn.com/shop/organization/baskets-organization/",
    "https://www.qvc.com/handbags-&-luggage/_/N-uoq0/c.html",
    "https://www.qvc.com/handbags-&-luggage/dooney-&-bourke/_/N-uoq0Z1z141jd/"
    "c.html",
    "https://www.qvc.com/handbags-&-luggage/handbags/_/N-2a0k5/c.html",
    "https://www.qvc.com/handbags-&-luggage/luggage/_/N-1cjuv/c.html",
    "https://www.redbubble.com/shop/drawstring-bags",
    "https://www.redbubble.com/shop/duffle-bags",
    "https://www.redbubble.com/shop/makeup+bags",
    "https://www.redbubble.com/shop/tote-bags",
    "https://www.rei.com/c/mens-sleeping-bags",
    "https://www.rei.com/c/womens-sleeping-bags",
    "https://www.rei.com/product/126821/patagonia-baggies-shorts-womens",
    "https://www.rei.com/rei-garage/c/sleeping-bags-and-accessories",
    "https://www.revolve.com/bags-clutches/br/cc5b36/",
    "https://www.revolve.com/bags-crossbody-bags/br/f8c179/",
    "https://www.revolve.com/bags-totes/br/317dcf/",
    "https://www.revolve.com/bags/br/2df9df/",
    "https://www.rockauto.com/es/cart/",
    "https://www.saksfifthavenue.com/c/handbags",
    "https://www.saksfifthavenue.com/c/handbags/handbags/crossbody-bags",
    "https://www.saksfifthavenue.com/c/handbags/handbags/totes",
    "https://www.saksfifthavenue.com/c/handbags/saint-laurent",
    "https://www.samsclub.com/b/gourmet-gift-baskets-and-food/1261",
    "https://www.samsclub.com/sams/controller/SamsInstaCartZipcodeController",
    "https://www.sephora.com/beauty-sample-bag",
    "https://www.sephora.com/beauty/beauty-sample-bag",
    "https://www.sephora.com/product/baggage-claim-gold-eye-masks-P428668",
    "https://www.sephora.com/shop/makeup-bags-cosmetic-bags",
    "https://www.shutterfly.com/maincart/start.sfly",
    "https://www.shutterfly.com/personalized-gifts/cotton-tote-bags",
    "https://www.shutterfly.com/signout/itemsInCart.sfly",
    "https://www.staples.com/hp-ink-cartridges/cat_CG812",
    "https://www.staples.com/services/printing/Cart",
    "https://www.staples.com/services/printing/CartCheckout",
    "https://www.staples.com/services/printing/cart",
    "https://www.sweetwater.com/c933--Electric_Guitar_Gig_Bags",
    "https://www.sweetwater.com/c982--Drum_Microphone_Bundles",
    "https://www.sweetwater.com/shop/drums-percussion/drum-cases-bags/",
    "https://www.sweetwater.com/store/detail/"
    "PS9339--prs-private-stock-number-9339-mccarty-594-hollowbody-ii-namm-2021-"
    "tiger-eye-glow",
    "https://www.talbots.com/accessories/handbags",
    "https://www.talbots.com/basket-weave-sweater---solid/P211121070.html",
    "https://www.talbots.com/nappa-havana-turnlock-camera-bag/P211061420.html",
    "https://www.talbots.com/on/demandware.store/Sites-talbotsus-Site/default/"
    "Cart-SubmitForm",
    "https://www.target.com/c/easter-basket-candy/-/N-4pa5f",
    "https://www.target.com/c/easter-basket-fillers/-/N-w48b8",
    "https://www.target.com/c/easter-baskets/-/N-4uswf",
    "https://www.teacherspayteachers.com/Product/"
    "Free-Easter-and-Spring-Craft-Chicks-in-a-Basket-1165442",
    "https://www.teacherspayteachers.com/Product/"
    "March-Madness-Basketball-Tournament-Math-Project-PBL-Digital-Google-"
    "Slides-326537",
    "https://www.teacherspayteachers.com/Store/Not-So-Wimpy-Teacher/Category/"
    "Writing-Bundles-509675",
    "https://www.therealreal.com/designers/chanel/women/handbags",
    "https://www.therealreal.com/shop/women/handbags",
    "https://www.therealreal.com/shop/women/handbags/crossbody-bags",
    "https://www.therealreal.com/shop/women/handbags/totes",
    "https://www.tractorsupply.com/tsc/catalog/egg-cartons",
    "https://www.tractorsupply.com/tsc/catalog/garden-carts",
    "https://www.tractorsupply.com/tsc/catalog/wheelbarrows-garden-carts",
    "https://www.tractorsupply.com/tsc/product/"
    "ohio-steel-15-cu-ft-poly-swivel-dump-cart",
    "https://www.ulta.com/"
    "free-dry-shampoo-9-piece-beauty-bag-with-50-hair-care-purchase",
    "https://www.ulta.com/makeup-bags-organizers",
    "https://www.underarmour.com/en-us/c/mens/accessories/bags/",
    "https://www.underarmour.com/en-us/c/mens/shoes/basketball/",
    "https://www.underarmour.com/en-us/c/mens/sports/basketball/",
    "https://www.underarmour.com/en-us/c/shoes/basketball/",
    "https://www.urbanoutfitters.com/bags-wallets-for-women",
    "https://www.urbanoutfitters.com/shop/"
    "bdg-bubble-corduroy-high-waisted-baggy-pant",
    "https://www.urbanoutfitters.com/shop/"
    "bdg-high-waisted-baggy-jean-medium-wash",
    "https://www.urbanoutfitters.com/shop/carter-triangle-bracket-wall-shelf",
    "https://www.vans.com/shop/mens-accessories-backpacks-bags",
    "https://www.vans.com/shop/womens-accessories-backpacks-bags",
    "https://www.walmart.com/browse/build-your-easter-basket/0/0/",
    "https://www.walmart.com/browse/home/bed-in-a-bag-sets/"
    "4044_539103_9474113_3388004",
    "https://www.wayfair.com/furniture/sb0/kitchen-islands-carts-c415182.html",
    "https://www.wayfair.com/storage-organization/sb0/"
    "hampers-laundry-baskets-c215044.html",
    "https://www.weightwatchers.com/us/recipe/"
    "2-ingredient-dough-everything-bagels/5fbd830687a4600722b90e01",
    "https://www.weightwatchers.com/us/recipe/"
    "corned-beef-and-cabbage-fried-rice/601c5ce2a1ceeb43bfb53608",
    "https://www.weightwatchers.com/us/recipe/"
    "corned-beef-and-cabbage-red-potatoes-1/5626a5f34236657004995b4d",
    "https://www.weightwatchers.com/us/recipe/"
    "spinach-bagels-herbed-cream-cheese/5e596fb716a786036659fb09",
    "https://www.westelm.com/products/hilo-basket-planters-d10517/",
    "https://www.westelm.com/products/"
    "mid-century-heathered-basketweave-wool-rug-steel-t1811/",
    "https://www.westelm.com/products/parker-mid-century-bar-cart-h415/",
    "https://www.westelm.com/shop/storage-organization/storage-baskets-bins/",
    "https://www.williams-sonoma.com/shop/easter/easter-feature-build-basket/",
    "https://www.williams-sonoma.com/shop/home-furniture/bar-carts-cabinets/",
    "https://www.williams-sonoma.com/shop/home-furniture/kitchen-island-cart/",
    "https://www.williams-sonoma.com/shop/tabletop-glassware-bar/"
    "serving-trays-platters-baskets/",
    "https://www.zappos.com/brooks-women-sneakers-athletic-shoes/"
    "CK_XARC81wFSARpaARrAAQHiAgUBAgsYCg.zso",
    "https://www.zappos.com/filters/women-sneakers-athletic-shoes/"
    "CK_XARC81wFaByTcA4Ql5AfAAQHiAgQBAgsY.zso",
    "https://www.zappos.com/sandals/CK_XARC51wHiAgIBAg.zso",
    "https://www.zappos.com/sneakers-athletic-shoes/CK_XARC81wHiAgIBAg.zso",
    "https://www2.hm.com/en_us/women/products/accessories/purses-bags.html",
    "https://www2.hm.com/en_us/women/products/pants/paperbag-pants.html",
    // Example non-cart URLs.
    "https://example.com/gp/aw/c?ref_=navm_hdr_cart",
    "https://example.com/cartoon",
    "https://example.com/checkout",
    "https://example.com/moving-cart",
    "https://example.com/shoulder_bag",
};

const char* kVisitCheckout[] = {
  "https://www.amazon.com/gp/buy/spc/handlers/display.html?",
  "https://smile.amazon.com/gp/buy/spc/handlers/display.html?",
  "https://pay.ebay.com/rgxo?action=view&sessionid=1111111111",
  "https://www.example.com/chkout/rc",
  "https://www.example.com/final-checkout",
  "https://www.example.com/Secure-Checkout",
  "https://www.example.com/Checkout-Begin",
  "https://www.example.com/checkout-start",
  "http://example.com/us/checkout/",
  "http://example.com/checkout/",
  "https://example.com/checkout?param#anchor",
  "https://example.com/checkout#anchor",
  "https://example.com/123/checkouts/456",
  "http://example.com/123/checkouts/456",
  "http://example.com/123/SPcheckouts",
  "https://www.example.com/intlcheckout"
};

const char* kNotVisitCheckout[] = {
  "https://www.amazon.com/gp/spc/handlers/display.html?",
  "https://www.amazon.com/gp/aw/c?ref_=navm_hdr_cart",
  "https://cart.ebay.com/rgxo",
  "https://pay.ebay.com/api/xo",
  "https://example.com/gp/cart/mobile/go-to-checkout.html/ref=ox_sc_proceed?proceedToCheckout=1",
  "http://example.com/checkoutput",
  "http://example.com/us/checkoutside/",
};

const char* kPurchaseURL[] = {
  "https://www.amazon.com/gp/buy/spc/handlers/static-submit-decoupled.html/ref=ox_spc_place_order?ie=UTF8",
};

const char* kNotPurchaseURL[] = {
  "https://example.com/gp/buy/spc/handlers/static-submit-decoupled.html/ref=ox_spc_place_order?ie=UTF8",
};

const char* kPurchaseText[] = {
  "Place order",
  "PAY NOW",
  "PLACE ORDER",
  "Pay now",
  "pay $24",
  "pay $24.35",
  "pay USD 24.35",
  "make payment",
  "confirm & pay",
  "PURCHASE",
  "MAKE THIS PURCHASE",
};

const char* kNotPurchaseText[] = {
  "I'd like to pay now",
  "replace order",
  "Pay nowadays",
  "make payment later",
  "confirm and continue"
  "submit",
  "Continue to Review Order",
  "next step: review order"
  "Process Order",
  "",
};

const char* kSkipText[] = {
  "Skipped",
  "A skipped product",
  "萬國碼",
  "一個萬國碼產品",
};

const char* kNotSkipText[] = {
  "",
  "A normal product",
};
// clang-format on

// Cannot use \b, or non-alphabetical words would fail.
const char kSkipPattern[] = "(^|\\W)(?i)(skipped|萬國碼)(\\W|$)";
std::map<std::string, std::string> kSkipParams = {
    {"product-skip-pattern", kSkipPattern}};

std::map<std::string, std::string> kSkipAddToCartRequests = {
    {"https://www.electronicexpress.com", "https://www.electronicexpress.com"},
    {"https://www.electronicexpress.com", "https://www.google.com"},
    {"https://www.costco.com", "https://www.clicktale.net"},
    {"https://www.hsn.com", "https://www.granify.com"},
    {"https://www.lululemon.com", "https://www.launchdarkly.com"},
    {"https://www.qvc.com", "https://www.qvc.com"},
    {"https://www.qvc.com", "https://www.google.com"},
};

std::map<std::string, std::string> kNotSkipAddToCartRequests = {
    {"https://www.costco.com", "https://www.granify.com"},
    {"https://www.hsn.com", "https://www.launchdarkly.com"},
    {"https://www.lululemon.com", "https://www.clicktale.net"},
};
}  // namespace

using cart::CommerceHintAgent;

class CommerceHintAgentUnitTest : public testing::Test {
 public:
  void TearDown() override {
    // Clear out heuristics data that is set up during testing.
    commerce_heuristics::CommerceHeuristicsData::GetInstance()
        .PopulateDataFromComponent(
            /*hint_json_data=*/"{}", /*global_json_data=*/"{}",
            /*product_id_json_data=*/"{}", /*cart_extraction_script=*/"");
  }

 protected:
  base::HistogramTester histogram_tester_;
};

TEST_F(CommerceHintAgentUnitTest, IsAddToCart) {
  // Heuristics from feature param default value.
  for (auto* str : kAddToCart) {
    EXPECT_TRUE(CommerceHintAgent::IsAddToCart(str)) << str;
  }
  for (auto* str : kNotAddToCart) {
    EXPECT_FALSE(CommerceHintAgent::IsAddToCart(str)) << str;
  }

  // Heuristics from component.
  const std::string& component_pattern = R"###(
      {
        "add_to_cart_request_regex": "bar"
      }
  )###";
  EXPECT_TRUE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                  .PopulateDataFromComponent("{}", component_pattern, "", ""));
  EXPECT_TRUE(CommerceHintAgent::IsAddToCart("request_bar"));
  for (auto* str : kAddToCart) {
    EXPECT_FALSE(CommerceHintAgent::IsAddToCart(str)) << str;
  }

  // Feature param has a higher priority.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
#if !BUILDFLAG(IS_ANDROID)
      ntp_features::kNtpChromeCartModule,
#else
      commerce::kCommerceHintAndroid,
#endif
      {{"add-to-cart-pattern", "foo"}});
  EXPECT_FALSE(CommerceHintAgent::IsAddToCart("request_bar"));
}

TEST_F(CommerceHintAgentUnitTest, IsAddToCartForDomBasedHeuristics) {
  for (auto* str : kAddToCartWithDOMBasedHeuristics) {
    EXPECT_TRUE(CommerceHintAgent::IsAddToCartForDomBasedHeuristics(str))
        << str;
  }
  for (auto* str : kNotAddToCartWithDOMBasedHeuristics) {
    EXPECT_FALSE(CommerceHintAgent::IsAddToCartForDomBasedHeuristics(str))
        << str;
  }
}

TEST_F(CommerceHintAgentUnitTest, IsAddToCart_SkipLengthLimit) {
  std::string str = "a";
  for (int i = 0; i < 12; ++i) {
    str += str;
  }
  // This is equal to length limit in CommerceHintAgent.
  EXPECT_EQ(str.size(), 4096U);

  str += "/add-to-cart";
  EXPECT_FALSE(CommerceHintAgent::IsAddToCart(str));
  EXPECT_TRUE(CommerceHintAgent::IsAddToCart(str, true));
}

TEST_F(CommerceHintAgentUnitTest, IsVisitCart) {
  // Heuristics from feature param default value.
  for (auto* str : kVisitCart) {
    EXPECT_TRUE(CommerceHintAgent::IsVisitCart(GURL(str))) << str;
  }
  for (auto* str : kNotVisitCart) {
    EXPECT_FALSE(CommerceHintAgent::IsVisitCart(GURL(str))) << str;
  }

  // General heuristics from component.
  const std::string& component_pattern = R"###(
      {
        "cart_page_url_regex": "bar"
      }
  )###";
  EXPECT_TRUE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                  .PopulateDataFromComponent("{}", component_pattern, "", ""));
  EXPECT_TRUE(CommerceHintAgent::IsVisitCart(GURL("https://wwww.foo.com/bar")));

  // Per-domain heuristics from component which has a higher priority than
  // general heuristics.
  EXPECT_TRUE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                  .PopulateDataFromComponent(R"###(
      {
          "foo.com": {
              "cart_url_regex" : "foo.com/([^/]+/)?trac"
          }
      }
  )###",
                                             "{}", "", ""));
  EXPECT_TRUE(
      CommerceHintAgent::IsVisitCart(GURL("https://wwww.foo.com/test/trac")));
  EXPECT_FALSE(
      CommerceHintAgent::IsVisitCart(GURL("https://wwww.foo.com/bar")));

  // Feature param has a higher priority than component.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
#if !BUILDFLAG(IS_ANDROID)
      ntp_features::kNtpChromeCartModule,
#else
      commerce::kCommerceHintAndroid,
#endif
      {{"cart-pattern", "baz"}, {"cart-pattern-mapping", R"###(
      {
        "foo.com": "foo.com/cart"
      }
  )###"}});
  EXPECT_FALSE(
      CommerceHintAgent::IsVisitCart(GURL("https://wwww.foo.com/bar")));
  EXPECT_FALSE(
      CommerceHintAgent::IsVisitCart(GURL("https://wwww.foo.com/test/trac")));
}

TEST_F(CommerceHintAgentUnitTest, IsVisitCheckout) {
  // Heuristics from feature param default value.
  for (auto* str : kVisitCheckout) {
    EXPECT_TRUE(CommerceHintAgent::IsVisitCheckout(GURL(str))) << str;
  }
  for (auto* str : kNotVisitCheckout) {
    EXPECT_FALSE(CommerceHintAgent::IsVisitCheckout(GURL(str))) << str;
  }
  histogram_tester_.ExpectBucketCount(
      "Commerce.Heuristics.CheckoutURLGeneralPatternSource",
      CommerceHeuristicsDataMetricsHelper::HeuristicsSource::FROM_COMPONENT, 0);
  EXPECT_GT(histogram_tester_.GetBucketCount(
                "Commerce.Heuristics.CheckoutURLGeneralPatternSource",
                CommerceHeuristicsDataMetricsHelper::HeuristicsSource::
                    FROM_FEATURE_PARAMETER),
            0);

  // General heuristics from component.
  const std::string& component_pattern = R"###(
      {
        "checkout_page_url_regex": "bar"
      }
  )###";
  EXPECT_TRUE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                  .PopulateDataFromComponent("{}", component_pattern, "", ""));
  EXPECT_TRUE(
      CommerceHintAgent::IsVisitCheckout(GURL("https://wwww.foo.com/bar")));
  histogram_tester_.ExpectBucketCount(
      "Commerce.Heuristics.CheckoutURLGeneralPatternSource",
      CommerceHeuristicsDataMetricsHelper::HeuristicsSource::FROM_COMPONENT, 1);
  // Per-domain heuristics from component which has a higher priority than
  // general heuristics.
  EXPECT_TRUE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                  .PopulateDataFromComponent(R"###(
      {
          "foo.com": {
              "checkout_url_regex" : "foo.com/([^/]+/)?tuokcehc"
          }
      }
  )###",
                                             "{}", "", ""));
  EXPECT_TRUE(CommerceHintAgent::IsVisitCheckout(
      GURL("https://wwww.foo.com/test/tuokcehc")));
  EXPECT_FALSE(
      CommerceHintAgent::IsVisitCheckout(GURL("https://wwww.foo.com/bar")));

  // Feature param has a higher priority than component.
  int prev_count = histogram_tester_.GetBucketCount(
      "Commerce.Heuristics.CheckoutURLGeneralPatternSource",
      CommerceHeuristicsDataMetricsHelper::HeuristicsSource::
          FROM_FEATURE_PARAMETER);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
#if !BUILDFLAG(IS_ANDROID)
      ntp_features::kNtpChromeCartModule,
#else
      commerce::kCommerceHintAndroid,
#endif
      {{"checkout-pattern", "foo"}, {"checkout-pattern-mapping", R"###(
      {
        "foo.com": "foo.com/checkout"
      }
  )###"}});
  EXPECT_FALSE(
      CommerceHintAgent::IsVisitCheckout(GURL("https://wwww.foo.com/bar")));
  EXPECT_FALSE(CommerceHintAgent::IsVisitCheckout(
      GURL("https://wwww.foo.com/test/tuokcehc")));
  EXPECT_EQ(2, histogram_tester_.GetBucketCount(
                   "Commerce.Heuristics.CheckoutURLGeneralPatternSource",
                   CommerceHeuristicsDataMetricsHelper::HeuristicsSource::
                       FROM_FEATURE_PARAMETER) -
                   prev_count);
}

TEST_F(CommerceHintAgentUnitTest, IsPurchaseByURL) {
  // Heuristics from feature param default value.
  for (auto* str : kPurchaseURL) {
    EXPECT_TRUE(CommerceHintAgent::IsPurchase(GURL(str))) << str;
  }
  for (auto* str : kNotPurchaseURL) {
    EXPECT_FALSE(CommerceHintAgent::IsPurchase(GURL(str))) << str;
  }

  // Per-domain heuristics from component has a higher priority than default
  // value.
  EXPECT_TRUE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                  .PopulateDataFromComponent(R"###(
      {
          "foo.com": {
              "purchase_url_regex" : "foo.com/([^/]+/)?esahcrup"
          }
      }
  )###",
                                             "{}", "", ""));
  EXPECT_TRUE(CommerceHintAgent::IsPurchase(
      GURL("https://wwww.foo.com/test/esahcrup")));

  // Feature param has a higher priority than component.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
#if !BUILDFLAG(IS_ANDROID)
      ntp_features::kNtpChromeCartModule,
#else
      commerce::kCommerceHintAndroid,
#endif
      {{"purchase-url-pattern-mapping", R"###(
      {
        "foo.com": "foo.com/purchase"
      }
  )###"}});
  EXPECT_FALSE(CommerceHintAgent::IsPurchase(
      GURL("https://wwww.foo.com/test/esahcrup")));
}

TEST_F(CommerceHintAgentUnitTest, IsPurchaseByForm) {
  // Heuristics from feature param default value.
  for (auto* str : kPurchaseText) {
    EXPECT_TRUE(CommerceHintAgent::IsPurchase(GURL(), str)) << str;
  }
  for (auto* str : kNotPurchaseText) {
    EXPECT_FALSE(CommerceHintAgent::IsPurchase(GURL(), str)) << str;
  }

  // Heuristics from component.
  const std::string& component_pattern = R"###(
      {
        "purchase_button_text_regex": "bar"
      }
  )###";
  EXPECT_TRUE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                  .PopulateDataFromComponent("{}", component_pattern, "", ""));
  EXPECT_TRUE(CommerceHintAgent::IsPurchase(GURL(), "bar"));
  for (auto* str : kPurchaseText) {
    EXPECT_FALSE(CommerceHintAgent::IsPurchase(GURL(), str)) << str;
  }

  // Feature param has a higher priority.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
#if !BUILDFLAG(IS_ANDROID)
      ntp_features::kNtpChromeCartModule,
#else
      commerce::kCommerceHintAndroid,
#endif
      {{"purchase-button-pattern", "foo"}});
  EXPECT_FALSE(CommerceHintAgent::IsPurchase(GURL(), "bar"));
}

TEST_F(CommerceHintAgentUnitTest, ShouldSkipFromFeatureParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
#if !BUILDFLAG(IS_ANDROID)
      ntp_features::kNtpChromeCartModule,
#else
      commerce::kCommerceHintAndroid,
#endif
      kSkipParams);

  for (auto* str : kSkipText) {
    EXPECT_TRUE(CommerceHintAgent::ShouldSkip(str)) << str;
  }
  for (auto* str : kNotSkipText) {
    EXPECT_FALSE(CommerceHintAgent::ShouldSkip(str)) << str;
  }
  int match_times = (sizeof(kSkipText) / sizeof(*kSkipText)) +
                    (sizeof(kNotSkipText) / sizeof(*kNotSkipText));
  ASSERT_EQ(histogram_tester_.GetBucketCount(
                "Commerce.Heuristics.SkipProductPatternSource",
                CommerceHeuristicsDataMetricsHelper::HeuristicsSource::
                    FROM_FEATURE_PARAMETER),
            match_times);
  ASSERT_EQ(histogram_tester_.GetBucketCount(
                "Commerce.Heuristics.SkipProductPatternSource",
                CommerceHeuristicsDataMetricsHelper::HeuristicsSource::
                    FROM_COMPONENT),
            0);
}

TEST_F(CommerceHintAgentUnitTest, ShouldSkipFromComponent) {
  const std::string& component_pattern = R"###(
      {
        "sensitive_product_regex": "(^|\\W)(?i)(skipped|萬國碼)(\\W|$)"
      }
  )###";
  EXPECT_TRUE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                  .PopulateDataFromComponent("{}", component_pattern, "", ""));

  for (auto* str : kSkipText) {
    EXPECT_TRUE(CommerceHintAgent::ShouldSkip(str)) << str;
  }
  for (auto* str : kNotSkipText) {
    EXPECT_FALSE(CommerceHintAgent::ShouldSkip(str)) << str;
  }
  int match_times = (sizeof(kSkipText) / sizeof(*kSkipText)) +
                    (sizeof(kNotSkipText) / sizeof(*kNotSkipText));
  ASSERT_EQ(histogram_tester_.GetBucketCount(
                "Commerce.Heuristics.SkipProductPatternSource",
                CommerceHeuristicsDataMetricsHelper::HeuristicsSource::
                    FROM_FEATURE_PARAMETER),
            0);
  ASSERT_EQ(histogram_tester_.GetBucketCount(
                "Commerce.Heuristics.SkipProductPatternSource",
                CommerceHeuristicsDataMetricsHelper::HeuristicsSource::
                    FROM_COMPONENT),
            match_times);
}

TEST_F(CommerceHintAgentUnitTest, ShouldSkip_Priority) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
#if !BUILDFLAG(IS_ANDROID)
      ntp_features::kNtpChromeCartModule,
#else
      commerce::kCommerceHintAndroid,
#endif
      kSkipParams);
  // Initiate component skip pattern so that nothing is skipped.
  const std::string& empty_pattern = R"###(
      {
        "sensitive_product_regex": "\\b\\B"
      }
  )###";
  EXPECT_TRUE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                  .PopulateDataFromComponent("{}", empty_pattern, "", ""));

  // Honor the skip pattern from feature parameter as it has higher priority.
  for (auto* str : kSkipText) {
    EXPECT_TRUE(CommerceHintAgent::ShouldSkip(str)) << str;
  }
  for (auto* str : kNotSkipText) {
    EXPECT_FALSE(CommerceHintAgent::ShouldSkip(str)) << str;
  }
}

TEST_F(CommerceHintAgentUnitTest, ShouldSkipAddToCartFromResource) {
  for (auto const& entry : kSkipAddToCartRequests) {
    EXPECT_TRUE(CommerceHintAgent::ShouldSkipAddToCartRequest(
        GURL(entry.first), GURL(entry.second)));
  }

  for (auto const& entry : kNotSkipAddToCartRequests) {
    EXPECT_FALSE(CommerceHintAgent::ShouldSkipAddToCartRequest(
        GURL(entry.first), GURL(entry.second)));
  }
}

float BenchmarkIsAddToCart(std::string_view str) {
  const base::TimeTicks now = base::TimeTicks::Now();
  for (int i = 0; i < kTestIterations; ++i) {
    CommerceHintAgent::IsAddToCart(str);
  }
  const base::TimeTicks end = base::TimeTicks::Now();
  float elapsed_us =
      static_cast<float>((end - now).InMicroseconds()) / kTestIterations;
  LOG(INFO) << "IsAddToCart(" << str.size() << " chars) took: " << elapsed_us
            << " µs";
  return elapsed_us;
}

float BenchmarkIsVisitCart(const GURL& url) {
  const base::TimeTicks now = base::TimeTicks::Now();
  for (int i = 0; i < kTestIterations; ++i) {
    CommerceHintAgent::IsVisitCart(url);
  }
  const base::TimeTicks end = base::TimeTicks::Now();
  float elapsed_us =
      static_cast<float>((end - now).InMicroseconds()) / kTestIterations;
  LOG(INFO) << "IsVisitCart(" << url.spec().size()
            << " chars) took: " << elapsed_us << " µs";
  return elapsed_us;
}

float BenchmarkIsVisitCheckout(const GURL& url) {
  const base::TimeTicks now = base::TimeTicks::Now();
  for (int i = 0; i < kTestIterations; ++i) {
    CommerceHintAgent::IsVisitCheckout(url);
  }
  const base::TimeTicks end = base::TimeTicks::Now();
  float elapsed_us =
      static_cast<float>((end - now).InMicroseconds()) / kTestIterations;
  LOG(INFO) << "IsVisitCheckout(" << url.spec().size()
            << " chars) took: " << elapsed_us << " µs";
  return elapsed_us;
}

float BenchmarkIsPurchase(const GURL& url, std::string_view str) {
  const base::TimeTicks now = base::TimeTicks::Now();
  for (int i = 0; i < kTestIterations; ++i) {
    CommerceHintAgent::IsPurchase(url, str);
  }
  const base::TimeTicks end = base::TimeTicks::Now();
  float elapsed_us =
      static_cast<float>((end - now).InMicroseconds()) / kTestIterations;
  LOG(INFO) << "IsPurchase(" << str.size() << " chars) took: " << elapsed_us
            << " µs";
  return elapsed_us;
}

float BenchmarkShouldSkip(std::string_view str) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
#if !BUILDFLAG(IS_ANDROID)
      ntp_features::kNtpChromeCartModule,
#else
      commerce::kCommerceHintAndroid,
#endif
      kSkipParams);

  const base::TimeTicks now = base::TimeTicks::Now();
  for (int i = 0; i < kTestIterations; ++i) {
    CommerceHintAgent::ShouldSkip(str);
  }
  const base::TimeTicks end = base::TimeTicks::Now();
  float elapsed_us =
      static_cast<float>((end - now).InMicroseconds()) / kTestIterations;
  LOG(INFO) << "ShouldSkip(" << str.size() << " chars) took: " << elapsed_us
            << " µs";
  return elapsed_us;
}

float BenchmarkShouldSkipAddToCart(const GURL& url) {
  const base::TimeTicks now = base::TimeTicks::Now();
  for (int i = 0; i < kTestIterations; ++i) {
    CommerceHintAgent::ShouldSkipAddToCartRequest(url, url);
  }
  const base::TimeTicks end = base::TimeTicks::Now();
  float elapsed_us =
      static_cast<float>((end - now).InMicroseconds()) / kTestIterations;
  LOG(INFO) << "ShouldSkipAddToCart(" << url.spec().size()
            << " chars) took: " << elapsed_us << " µs";
  return elapsed_us;
}

// TSAN builds are 20~50X slower than Release build.
#if defined(THREAD_SANITIZER) || defined(ADDRESS_SANITIZER) ||             \
    defined(MEMORY_SANITIZER) || BUILDFLAG(CFI_CAST_CHECK) ||              \
    BUILDFLAG(CFI_ICALL_CHECK) || BUILDFLAG(CFI_ENFORCEMENT_DIAGNOSTIC) || \
    BUILDFLAG(CFI_ENFORCEMENT_TRAP)
#define MAYBE_RegexBenchmark DISABLED_RegexBenchmark
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || !defined(NDEBUG)
// TODO(crbug.com/353775530):  Enable once cause of general slowdown is fixed.
#define MAYBE_RegexBenchmark DISABLED_RegexBenchmark
#else
#define MAYBE_RegexBenchmark RegexBenchmark
#endif

TEST_F(CommerceHintAgentUnitTest, MAYBE_RegexBenchmark) {
  std::string str = "abcdefghijklmnop";
  const GURL basic_url = GURL("http://example.com/");
  // Compile regex before benchmark loop.
  CommerceHintAgent::IsAddToCart(str);
  CommerceHintAgent::IsVisitCart(basic_url);
  CommerceHintAgent::IsVisitCheckout(basic_url);
  CommerceHintAgent::IsPurchase(basic_url, str);
  for (int length = 16; length <= (1L << 20); length *= 4) {
    const GURL url("http://example.com/" + str);

    // With string copy, it would take at least 400us, assuming string length is
    // 1M chars, and running x86-64 Release build on Linux workstation. x86-64
    // Debug build is ~4X slower, Debug build on Pixel 2 is 3~10X slower, and
    // Release build on Nexus 5 is ~10X slower.
    int slow_factor = 1;
#if !defined(NDEBUG)
    slow_factor *= 4;
#endif
#if BUILDFLAG(IS_ANDROID)
    slow_factor *= 10;
#endif

    float elapsed_us = BenchmarkIsAddToCart(str);
    // Typical value is ~10us on x86-64 Release build.
    // Without capping the length, it would take at least 2000us.
    EXPECT_LT(elapsed_us, 50.0 * slow_factor);

    elapsed_us = BenchmarkIsVisitCart(url);
    // Typical value is ~10us.
    EXPECT_LT(elapsed_us, 50.0 * slow_factor);

    elapsed_us = BenchmarkIsVisitCheckout(url);
    // Typical value is ~10us.
    // Without capping the length, it would take at least 2000us.
    EXPECT_LT(elapsed_us, 100.0 * slow_factor);

    elapsed_us = BenchmarkIsPurchase(basic_url, str);
    // Typical value is ~0.1us.
    EXPECT_LT(elapsed_us, 5.0 * slow_factor);

    elapsed_us = BenchmarkShouldSkip(str);
    // Typical value is ~10us.
    EXPECT_LT(elapsed_us, 50.0 * slow_factor);

    elapsed_us = BenchmarkShouldSkipAddToCart(url);
    // Typical value is ~10us.
    EXPECT_LT(elapsed_us, 50.0 * slow_factor);

    str += str;
    str += str;
  }
}
